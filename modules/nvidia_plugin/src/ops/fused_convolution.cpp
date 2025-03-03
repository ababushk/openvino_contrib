// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <fmt/format.h>

#include <error.hpp>
#include <exception>
#include <gsl/gsl_assert>
#include <memory>

#include "convolution_components/convolution_cudnn_components.hpp"
#include "cuda_operation_registry.hpp"
#include "fused_convolution_cudnn.hpp"
#include "fused_convolution_cudnn_decomposed.hpp"
#include "transformer/nodes/cuda_plugin_custom_node_types.hpp"
#ifdef ENABLE_CUDNN_BACKEND_API
#include "fused_convolution_cudnn_be.hpp"
#endif  // ENABLE_CUDNN_BACKEND_API

namespace ov {
namespace nvidia_gpu {

OperationBase::Ptr fusedConvolutionFactory(const CreationContext& context,
                                           const std::shared_ptr<ov::Node>& node,
                                           OperationBase::IndexCollection&& inputIds,
                                           OperationBase::IndexCollection&& outputIds) {
    using ArgIndices = Convolution::Details::FusedConvolutionIndices;
    using IndexCollection = OperationBase::IndexCollection;
    const auto element_type = node->get_input_element_type(ArgIndices::input);
    Expects(element_type == node->get_input_element_type(ArgIndices::filter));
    Expects(element_type == node->get_input_element_type(ArgIndices::bias));
    Expects(element_type == node->get_output_element_type(ArgIndices::output));
    const bool includesOnlyBiasAdd = node->inputs().size() == 3;
    const bool includesSecondAddition = node->inputs().size() == 4;
    Expects(includesOnlyBiasAdd || includesSecondAddition);  // Conv input, filters, Bias and optional Add

    std::stringstream exception_msg;
    const auto fused_conv = std::dynamic_pointer_cast<nodes::FusedConvolution>(node);
    const auto fused_group_conv = std::dynamic_pointer_cast<nodes::FusedGroupConvolution>(node);
    Expects(fused_conv || fused_group_conv);

    const auto params = fused_conv ? Convolution::Details::FusedConvolutionParams{*fused_conv}
                                   : Convolution::Details::FusedConvolutionParams{*fused_group_conv};

#ifdef ENABLE_CUDNN_BACKEND_API
    const bool should_try_backend = node->get_type_name() == std::string("FusedConvolution");
    if (should_try_backend) {
        try {
            return std::make_shared<FusedConvolutionCuDnnBE>(
                context, *node, IndexCollection{inputIds}, IndexCollection{outputIds}, params);
        } catch (const std::exception& e) {
            exception_msg << fmt::format(
                "unsupported `{}` node: Failed to create "
                "FusedConvolutionCuDnnBE impl: {}",
                node->get_type_info().name,
                e.what());
        }
    }
#endif  // ENABLE_CUDNN_BACKEND_API

    const auto conv_descs{std::make_shared<Convolution::Details::ConvolutionDescriptorsCuDnn>(context, params.conv_)};
    const auto bias_desc{Convolution::Details::MakeFusedAddDescriptor(params.bias_shape_, params.conv_.element_type_)};
    const auto activation_desc{Convolution::Details::MakeFusedActivationDescriptor(params.activation_)};
    const auto add_desc{params.add_shape_ ? Convolution::Details::MakeFusedAddDescriptor(params.add_shape_.value(),
                                                                                         params.conv_.element_type_)
                                          : nullptr};

    // cudnnConvolutionBiasActivationForward() doesn't work properly with CUDNN_ACTIVATION_IDENTITY and any algorithm
    // other than CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM, so we should decompose the convolution node and call
    // separate cuDNN functions.
    // For more information see:
    // https://docs.nvidia.com/deeplearning/cudnn/api/index.html#cudnnConvolutionBiasActivationForward
    const bool should_decompose = params.activation_ == ov::nvidia_gpu::nodes::ActivationMode::NO_ACTIVATION &&
                                  conv_descs->Algo().algo != CUDNN_CONVOLUTION_FWD_ALGO_IMPLICIT_PRECOMP_GEMM;

    if (should_decompose) {
        try {
            return std::make_shared<FusedConvolutionCuDnnDecomposed>(context,
                                                                     *node,
                                                                     IndexCollection{inputIds},
                                                                     IndexCollection{outputIds},
                                                                     conv_descs,
                                                                     bias_desc,
                                                                     add_desc,
                                                                     activation_desc);
        } catch (const std::exception& e) {
            throwIEException(
                fmt::format("unsupported `{}` node: Failed to create "
                            "FusedConvolutionCuDnnDecomposed impl: {}",
                            node->get_type_info().name,
                            e.what()));
        }
    }

    try {
        return std::make_shared<FusedConvolutionCuDnn>(context,
                                                       *node,
                                                       IndexCollection{inputIds},
                                                       IndexCollection{outputIds},
                                                       conv_descs,
                                                       bias_desc,
                                                       add_desc,
                                                       activation_desc);
    } catch (const std::exception& e) {
        exception_msg << fmt::format(
            "unsupported `{}` node: Failed to create "
            "FusedConvolutionCuDnn impl: {}",
            node->get_type_info().name,
            e.what());
    }

    throwIEException(fmt::format("Convolution node is not supported:\n{}", exception_msg.str()));
}

OPERATION_REGISTER_FACTORY(fusedConvolutionFactory, FusedConvolution);
OPERATION_REGISTER_FACTORY(fusedConvolutionFactory, FusedGroupConvolution);

}  // namespace nvidia_gpu
}  // namespace ov
