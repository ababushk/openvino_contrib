// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "less.hpp"

#include <cuda_operation_registry.hpp>

namespace ov {
namespace nvidia_gpu {

LessOp::LessOp(const CreationContext& context,
               const ov::Node& node,
               IndexCollection&& inputIds,
               IndexCollection&& outputIds)
    : Comparison(context, node, std::move(inputIds), std::move(outputIds), kernel::Comparison::Op_t::LESS) {}

OPERATION_REGISTER(LessOp, Less);

}  // namespace nvidia_gpu
}  // namespace ov
