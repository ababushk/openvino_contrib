// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cuda/runtime.hpp>
#include <ngraph/function.hpp>

#include "cpp/ie_cnn_network.h"
#include "cuda_config.hpp"

namespace ov {
namespace nvidia_gpu {

class GraphTransformer {
public:
    std::shared_ptr<ngraph::Function> export_transform(const CUDA::Device& device,
                                                       const std::shared_ptr<const ngraph::Function>& function,
                                                       const InferenceEngine::InputsDataMap& inputInfoMap,
                                                       const InferenceEngine::OutputsDataMap& outputsInfoMap,
                                                       const Configuration& config) const;
    /**
     * @brief Transform takes an ngraph::Function and applies all the necessary
     *        graph transofrmations to achieve the maximum optimization of the
     *        model for execution on a CUDA device. The transformations may
     *        include CUDA-specific op fusions and some common OpenVino
     *        transformations as well.
     * @param function a valid shared ptr to a model, represented as an
     *        ngraph::Function instance.
     * @param config a string-string map of configuration for loading an
     * executable network (e.g. a model); this config influences on what exact
     *        transformations are being applied to the original graph.
     * @return an ngraph::Function containing only the CUDA-optimized operations.
     */
    std::shared_ptr<ngraph::Function> transform(const CUDA::Device& device,
                                                const std::shared_ptr<const ngraph::Function>& function,
                                                const InferenceEngine::InputsDataMap& inputInfoMap,
                                                const InferenceEngine::OutputsDataMap& outputsInfoMap,
                                                const Configuration& config) const;
};

}  // namespace nvidia_gpu
}  // namespace ov
