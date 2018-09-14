/*
 * Copyright 2018 Codeplay Software Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SYCLDNN_INCLUDE_POOLING_ADD_PADDING_TO_PARAMS_H_
#define SYCLDNN_INCLUDE_POOLING_ADD_PADDING_TO_PARAMS_H_

/**
 * \file
 * Contains helper functions to add the padding and output sizes to a
 * \ref sycldnn::pooling::PoolingParams parameter struct from the input sizes,
 * window sizes and strides.
 */
#include "sycldnn/padding_mode.h"

#include "sycldnn/helpers/padding.h"

#include "sycldnn/pooling/params.h"

namespace sycldnn {
namespace pooling {
namespace helpers {

/**
 * Add the padding and output sizes to a pooling parameter struct from the input
 * sizes, window sizes and strides.
 * \param params The convolution parameters that the output will be based
 *               on.
 * \param type The type of padding that should be used to calculate the actual
 *             size of padding to be used in the convolution.
 * \return The original params, modified with the padding sizes required.
 */
pooling::PoolingParams add_padding_to(pooling::PoolingParams params,
                                      PaddingMode type) {
  auto row_padding = sycldnn::helpers::calculate_padding(
      params.in_rows, params.window_rows, params.stride_rows, type);
  params.out_rows = row_padding.output;
  params.pad_rows = row_padding.padding;

  auto col_padding = sycldnn::helpers::calculate_padding(
      params.in_cols, params.window_cols, params.stride_cols, type);
  params.out_cols = col_padding.output;
  params.pad_cols = col_padding.padding;

  return params;
}
}  // namespace helpers
}  // namespace pooling
}  // namespace sycldnn
#endif  // SYCLDNN_INCLUDE_POOLING_ADD_PADDING_TO_PARAMS_H_
