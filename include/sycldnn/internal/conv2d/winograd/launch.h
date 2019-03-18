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
#ifndef SYCLDNN_INCLUDE_INTERNAL_CONV2D_WINOGRAD_LAUNCH_H_
#define SYCLDNN_INCLUDE_INTERNAL_CONV2D_WINOGRAD_LAUNCH_H_

#include "sycldnn/status.h"

#include "sycldnn/conv2d/params.h"

#include "sycldnn/internal/conv2d/batch_info.h"
#include "sycldnn/internal/conv2d/internal_pointer_set.h"

#include "sycldnn/internal/conv2d/winograd/calculate_offsets.h"
#include "sycldnn/internal/conv2d/winograd/kernel_params.h"
#include "sycldnn/internal/conv2d/winograd/launch_filter_transform.h"
#include "sycldnn/internal/conv2d/winograd/launch_input_transform.h"
#include "sycldnn/internal/conv2d/winograd/launch_output_transform.h"
#include "sycldnn/internal/conv2d/winograd/pointer_set.h"
#include "sycldnn/internal/conv2d/winograd/tile_info.h"

#include <CL/sycl.hpp>

/**
 * \file
 * Contains the internal launcher sycldnn::conv2d::internal::winograd::launch()
 * for a Winograd convolution, which will allocate any required temporary
 * buffers, call the transform kernels and use the provided Backend's batch
 * matrix multiplication to compute the convolution.
 */

namespace sycldnn {
namespace conv2d {
/** Namespace containing internal implementation details for conv2d. */
namespace internal {
/** Namespace containing internal Winograd implementation details. */
namespace winograd {

/**
 * Launch the kernels to compute a convolution over all minibatches.
 *
 * \param pointers   Full set of pointers for the convolution
 * \param params     Kernel parameters for the convolution
 * \param tile_info  Information about the number of Winograd tiles
 * \param batch_info Information about the minibatch size
 * \param backend    Backend to use for matrix multiplication
 * \return An SNNStatus object containing a SYCL event corresponding to the last
 * kernel launched.
 */
template <
    typename T, int M, int N, int R, int S, typename ConvType, typename Backend,
    typename std::enable_if<
        !std::is_same<ConvType, conv_type::FilterBackprop>::value, int>::type =
        0>
SNNStatus launch_with_transforms(FullPointerSet<T, Backend> const& pointers,
                                 Conv2DParams const& params,
                                 TileInfo const& tile_info,
                                 BatchInfo const& batch_info,
                                 Backend& backend) {
  constexpr int A = M + R - 1;
  constexpr int B = N + S - 1;
  constexpr bool transpose_input = false;
  // Need to transpose for the input backprop, but not for the forward pass
  constexpr bool transpose_filter =
      std::is_same<ConvType, conv_type::InputBackprop>::value;
  auto fil_status = launch_filter_transform<T, ConvType, M, N, R, S>(
      pointers.filter, pointers.filter_transform, params, tile_info, backend);
  if (fil_status.status != StatusCode::OK) {
    return fil_status;
  }

  cl::sycl::event last_event;
  Conv2DParams kernel_params{params};
  kernel_params.batch = batch_info.images_per_batch;
  for (size_t i = 0; i < batch_info.n_batches; ++i) {
    auto offset =
        calculate_offsets<ConvType>(i, batch_info.images_per_batch, params);
    if (i == batch_info.n_batches - 1) {
      kernel_params.batch = batch_info.last_batch_size;
    }

    auto inp_status = launch_input_transform<T, ConvType, M, N, R, S>(
        pointers.input + offset.in, pointers.input_transform, kernel_params,
        tile_info, backend);
    if (inp_status.status != StatusCode::OK) {
      return inp_status;
    }

    backend.template batch_matmul<transpose_input, transpose_filter, T>(
        pointers.input_transform, pointers.filter_transform,
        pointers.intermediate, A * B, tile_info.number * kernel_params.batch,
        kernel_params.channels, kernel_params.features);

    auto out_status = launch_output_transform<T, ConvType, M, N, R, S>(
        pointers.intermediate, pointers.output + offset.out, kernel_params,
        tile_info, backend);
    if (out_status.status != StatusCode::OK) {
      return out_status;
    }
    last_event = out_status.event;
  }
  return SNNStatus{last_event, StatusCode::OK};
}

/** \copydoc launch_with_transforms() */
template <
    typename T, int M, int N, int R, int S, typename ConvType, typename Backend,
    typename std::enable_if<
        std::is_same<ConvType, conv_type::FilterBackprop>::value, int>::type =
        0>
SNNStatus launch_with_transforms(FullPointerSet<T, Backend> pointers,
                                 Conv2DParams const& params,
                                 TileInfo const& tile_info,
                                 BatchInfo const& batch_info,
                                 Backend& backend) {
  constexpr int A = M + R - 1;
  constexpr int B = N + S - 1;
  constexpr bool transpose_input = true;
  constexpr bool transpose_filter = false;
  // For the filter backprop, we need to switch the temporary filter
  // transform buffer and the temporary intermediate buffer. The filter
  // backprop convolution essentially uses the original output as a filter,
  // with the output being written to a tensor the same size as the original
  // filter.
  std::swap(pointers.filter_transform, pointers.intermediate);

  cl::sycl::event last_event;
  Conv2DParams kernel_params{params};
  kernel_params.batch = batch_info.images_per_batch;
  for (size_t i = 0; i < batch_info.n_batches; ++i) {
    auto offset =
        calculate_offsets<ConvType>(i, batch_info.images_per_batch, params);

    if (i == batch_info.n_batches - 1) {
      kernel_params.batch = batch_info.last_batch_size;
    }
    auto inp_status = launch_input_transform<T, ConvType, M, N, R, S>(
        pointers.input + offset.in, pointers.input_transform, kernel_params,
        tile_info, backend);
    if (inp_status.status != StatusCode::OK) {
      return inp_status;
    }

    auto fil_status = launch_filter_transform_filter_backprop<T, M, N, R, S>(
        pointers.filter + offset.out, pointers.filter_transform, kernel_params,
        tile_info, backend);
    if (fil_status.status != StatusCode::OK) {
      return fil_status;
    }

    backend.template batch_matmul<transpose_input, transpose_filter, T>(
        pointers.input_transform, pointers.filter_transform,
        pointers.intermediate, A * B, kernel_params.channels,
        tile_info.number * kernel_params.batch, kernel_params.features);

    if (i == 0) {
      // For the first mini-batch we want to overwrite the output buffer
      auto out_status =
          launch_output_transform_filter_backprop<T, M, N, R, S, false>(
              pointers.intermediate, pointers.output, kernel_params, tile_info,
              backend);
      if (out_status.status != StatusCode::OK) {
        return out_status;
      }
      last_event = out_status.event;
    } else {
      // For subsequent mini-batches we need to accumulate the results with
      // those already in the output buffer
      auto out_status =
          launch_output_transform_filter_backprop<T, M, N, R, S, true>(
              pointers.intermediate, pointers.output, kernel_params, tile_info,
              backend);
      if (out_status.status != StatusCode::OK) {
        return out_status;
      }
      last_event = out_status.event;
    }
  }
  return SNNStatus{last_event, StatusCode::OK};
}

/**
 * Convert the user provided pointers into internal pointers, allocate any
 * required temporary buffers, compute the Winograd tile sizes and then launch
 * the convolution with launch_with_transforms().
 *
 * \param input   User provided input pointer
 * \param filter  User provided filter pointer
 * \param output  User provided output pointer
 * \param params  User provided convolution parameters
 * \param backend User provided backend to handle allocations and matrix
 *                multiplies
 * \return An SNNStatus object containing a SYCL event corresponding to the last
 * kernel launched.
 */
template <typename T, typename ConvType, int M, int N, int R, int S,
          typename Backend>
SNNStatus launch_with_tiles(
    typename Backend::template pointer_type<T const> input,
    typename Backend::template pointer_type<T const> filter,
    typename Backend::template pointer_type<T> output,
    Conv2DParams const& params, Backend& backend) {
  constexpr int A = M + R - 1;
  constexpr int B = N + S - 1;
  auto kernel_params = get_params<ConvType>(params);
  InternalPointerSet<T, Backend> input_pointers{input, filter, output, backend};
  auto const tile_info = get_tile_info<ConvType, M, N, R, S>(kernel_params);
  AllocatedPointerSet<T, Backend> allocated_pointers{
      input_pointers, kernel_params, A * B, tile_info, backend};
  auto batch_info =
      get_batch_info(allocated_pointers.minibatch_size, params.batch);

  return launch_with_transforms<T, M, N, R, S, ConvType>(
      allocated_pointers.to_full_pointer_set(), kernel_params, tile_info,
      batch_info, backend);
}

/**
 * Launch a Winograd convolution. Match up the runtime parameters to the
 * available Winograd tile sizes and launch those kernels using
 * launch_with_tiles().
 *
 * \param input   User provided input pointer
 * \param filter  User provided filter pointer
 * \param output  User provided output pointer
 * \param params  User provided convolution parameters
 * \param backend User provided backend to handle allocations and matrix
 *                multiplies
 * \return An SNNStatus object containing a SYCL event corresponding to the last
 * kernel launched.
 */
template <typename T, typename ConvType, typename Backend,
          typename std::enable_if<
              !std::is_same<ConvType, conv_type::FilterBackprop>::value,
              int>::type = 0>
SNNStatus launch(typename Backend::template pointer_type<T const> input,
                 typename Backend::template pointer_type<T const> filter,
                 typename Backend::template pointer_type<T> output,
                 Conv2DParams const& params, Backend& backend) {
  if (params.window_rows == 3 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 2, 2, 3, 3>(input, filter, output,
                                                      params, backend);
  }
  if (params.window_rows == 3 && params.window_cols == 1) {
    return launch_with_tiles<T, ConvType, 2, 1, 3, 1>(input, filter, output,
                                                      params, backend);
  }
  if (params.window_rows == 1 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 1, 2, 1, 3>(input, filter, output,
                                                      params, backend);
  }
  return SNNStatus{{}, StatusCode::InvalidAlgorithm};
}

/** \copydoc sycldnn::conv2d::internal::winograd::launch() */
template <typename T, typename ConvType, typename Backend,
          typename std::enable_if<
              std::is_same<ConvType, conv_type::FilterBackprop>::value,
              int>::type = 0>
SNNStatus launch(typename Backend::template pointer_type<T const> input,
                 typename Backend::template pointer_type<T const> filter,
                 typename Backend::template pointer_type<T> output,
                 Conv2DParams const& params, Backend& backend) {
  if (params.window_rows == 3 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 3, 3, 2, 2>(input, filter, output,
                                                      params, backend);
  }
  if (params.window_rows == 3 && params.window_cols == 1) {
    return launch_with_tiles<T, ConvType, 3, 1, 2, 1>(input, filter, output,
                                                      params, backend);
  }
  if (params.window_rows == 1 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 1, 3, 1, 2>(input, filter, output,
                                                      params, backend);
  }
  return SNNStatus{{}, StatusCode::InvalidAlgorithm};
}

/** \copydoc sycldnn::conv2d::internal::winograd::launch() */
template <typename T, typename ConvType, typename Backend,
          typename std::enable_if<
              !std::is_same<ConvType, conv_type::FilterBackprop>::value,
              int>::type = 0>
SNNStatus launch_large(typename Backend::template pointer_type<T const> input,
                       typename Backend::template pointer_type<T const> filter,
                       typename Backend::template pointer_type<T> output,
                       Conv2DParams const& params, Backend& backend) {
  if (params.window_rows == 3 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 4, 4, 3, 3>(input, filter, output,
                                                      params, backend);
  }
  return SNNStatus{{}, StatusCode::InvalidAlgorithm};
}

/** \copydoc sycldnn::conv2d::internal::winograd::launch() */
template <typename T, typename ConvType, typename Backend,
          typename std::enable_if<
              std::is_same<ConvType, conv_type::FilterBackprop>::value,
              int>::type = 0>
SNNStatus launch_large(typename Backend::template pointer_type<T const> input,
                       typename Backend::template pointer_type<T const> filter,
                       typename Backend::template pointer_type<T> output,
                       Conv2DParams const& params, Backend& backend) {
  if (params.window_rows == 3 && params.window_cols == 3) {
    return launch_with_tiles<T, ConvType, 3, 3, 3, 3>(input, filter, output,
                                                      params, backend);
  }
  return SNNStatus{{}, StatusCode::InvalidAlgorithm};
}

}  // namespace winograd
}  // namespace internal
}  // namespace conv2d
}  // namespace sycldnn

#endif  // SYCLDNN_INCLUDE_INTERNAL_CONV2D_WINOGRAD_ALLOC_INFO_H_
