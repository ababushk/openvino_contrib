// Copyright (C) 2018-2020 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "cuda_memory_manager.hpp"

#include <details/ie_exception.hpp>

#include "cuda_operation_base.hpp"

namespace CUDAPlugin {

MemoryManager::MemoryManager(std::shared_ptr<DeviceMemBlock> immutableTensors,
                             MemoryModel::Ptr mutableMemoryModel)
  : immutable_tensors_{ immutableTensors },
    mutable_tensors_{ std::make_unique<DeviceMemBlock>(mutableMemoryModel) }
{
}

MemoryManager::InputTensors
MemoryManager::inputTensorPointers(const IOperationMeta& operation) {
  InputTensors result;
  for (auto id : operation.GetInputIds()) {
    const void* ptr = immutable_tensors_->deviceTensorPtr(id);
    if (ptr == nullptr)
      ptr = mutable_tensors_->deviceTensorPtr(id);

    IE_ASSERT(ptr != nullptr) << "Tensor not found. ID is " << id;
    result.emplace_back(ptr);
  }
  return result;
}

MemoryManager::OutputTensors
MemoryManager::outputTensorPointers(const IOperationMeta& operation) {
  OutputTensors result;
  for (auto id : operation.GetOutputIds()) {
    void* ptr = mutable_tensors_->deviceTensorPtr(id);

    IE_ASSERT(ptr != nullptr) << "Tensor not found. ID is " << id;
    result.emplace_back(ptr);
  }
  return result;
}

}  // namespace CUDAPlugin