// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cutensor.h>

#include "runtime.hpp"

inline void throwIfError(
    cutensorStatus_t err,
    const std::experimental::source_location& location = std::experimental::source_location::current()) {
    if (err != CUTENSOR_STATUS_SUCCESS) CUDAPlugin::throwIEException(cutensorGetErrorString(err), location);
}

inline void logIfError(
    cutensorStatus_t err,
    const std::experimental::source_location& location = std::experimental::source_location::current()) {
    if (err != CUTENSOR_STATUS_SUCCESS) CUDAPlugin::logError(cutensorGetErrorString(err), location);
}

namespace CUDA {

class CuTensorHandle : public Handle<cutensorHandle_t> {
public:
    CuTensorHandle() : Handle(cutensorInit, nullptr) {}
};

}  // namespace CUDA
