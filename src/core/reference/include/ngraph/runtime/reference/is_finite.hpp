// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <cmath>
#include <cstddef>

namespace ngraph {
namespace runtime {
namespace reference {
template <typename T, typename U>
typename std::enable_if<std::is_floating_point<T>::value, void>::type is_finite(const T* input,
                                                                                U* output,
                                                                                size_t count) {
    std::transform(input, input + count, output, [](T x) -> U {
        return std::isfinite(x);
    });
}

// used for float16 and bfloat 16 datatypes
template <typename T, typename U>
typename std::enable_if<std::is_class<T>::value, void>::type is_finite(const T* input, U* output, size_t count) {
    std::transform(input, input + count, output, [](T x) -> U {
        return std::isfinite(static_cast<float>(x));
    });
}

}  // namespace reference
}  // namespace runtime
}  // namespace ngraph
