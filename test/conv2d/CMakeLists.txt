# Copyright 2018 Codeplay Software Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use these files except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required(VERSION 3.2.2)

include(HandleGTest)
include(SNNHelpers)

snn_test(
  WITH_SYCL
  TARGET
    simple_convolution
  SIZE
    short
  SOURCES
    convolutions.cc
  OBJECTS
    $<TARGET_OBJECTS:direct_conv2d>
    $<TARGET_OBJECTS:tiled_conv2d>
    $<TARGET_OBJECTS:im2col_conv2d>
    $<TARGET_OBJECTS:winograd_conv2d>
    $<TARGET_OBJECTS:matmul>
)

foreach(_type IN ITEMS "forward" "input_backprop" "filter_backprop")
  snn_test(
    WITH_SYCL
    TARGET
      conv2d_${_type}_1x1
    SIZE
      short
    SOURCES
      ${_type}_1x1.cc
    OBJECTS
      $<TARGET_OBJECTS:direct_conv2d>
      $<TARGET_OBJECTS:tiled_conv2d>
      $<TARGET_OBJECTS:im2col_conv2d>
      $<TARGET_OBJECTS:winograd_conv2d>
      $<TARGET_OBJECTS:matmul>
  )
endforeach()

snn_test(
  WITH_SYCL
  TARGET
    convolution_offsets
  SIZE
    short
  SOURCES
    convolution_offsets.cc
  OBJECTS
    $<TARGET_OBJECTS:direct_conv2d>
    $<TARGET_OBJECTS:tiled_conv2d>
    $<TARGET_OBJECTS:im2col_conv2d>
    $<TARGET_OBJECTS:winograd_conv2d>
    $<TARGET_OBJECTS:matmul>
)
snn_test(
  WITH_SYCL
  TARGET
    workspace_comparative
  SIZE
    moderate
  SOURCES
    workspace_comparative_test.cc
  OBJECTS
    $<TARGET_OBJECTS:direct_conv2d>
    $<TARGET_OBJECTS:tiled_conv2d>
    $<TARGET_OBJECTS:im2col_conv2d>
    $<TARGET_OBJECTS:winograd_conv2d>
    $<TARGET_OBJECTS:matmul>
)

set(_cxx_opts CXX_OPTS)
set(_matmul_providers)
if(SNN_TEST_EIGEN_MATMULS)
  include(HandleEigen)
  list(APPEND _cxx_opts -DSNN_TEST_EIGEN_MATMULS=1)
  list(APPEND _matmul_providers Eigen::Eigen)
endif()

if(SNN_TEST_SYCLBLAS_MATMULS)
  include(HandleSyclBLAS)
  list(APPEND _cxx_opts -DSNN_TEST_SYCLBLAS_MATMULS=1)
  list(APPEND _matmul_providers SyclBLAS::SyclBLAS)
endif()

if(SNN_TEST_CLBLAST_MATMULS)
  include(HandleCLBlast)
  list(APPEND _cxx_opts -DSNN_TEST_CLBLAST_MATMULS=1)
  list(APPEND _matmul_providers clblast)
endif()

set(_windows 1 1 3 3 5 5 7 7 11 11)
set(_strides 1 2 1 2 1 2 1 4 1 4)
list(LENGTH _windows _list_len_raw)
math(EXPR _list_len "${_list_len_raw} - 1")
foreach(_index RANGE ${_list_len})
  list(GET _windows ${_index} _window)
  list(GET _strides ${_index} _stride)
  foreach(_type IN ITEMS "forward" "input_backprop" "filter_backprop")
    if(SNN_TEST_EIGEN_MATMULS OR SNN_TEST_SYCLBLAS_MATMULS)
      set(_kernel_src KERNEL_SOURCES
        ${_type}_window${_window}_stride${_stride}.cc
      )
    endif()
    snn_test(
      WITH_SYCL
      HIGH_MEM
      TARGET
        conv2d_${_type}_window${_window}_stride${_stride}
      SIZE
        moderate
      SOURCES
        ${_type}_window${_window}_stride${_stride}.cc
      OBJECTS
        $<TARGET_OBJECTS:direct_conv2d>
        $<TARGET_OBJECTS:tiled_conv2d>
        $<TARGET_OBJECTS:im2col_conv2d>
        $<TARGET_OBJECTS:winograd_conv2d>
        $<TARGET_OBJECTS:matmul>
      PUBLIC_LIBRARIES
        ${_matmul_providers}
      ${_cxx_opts}
      ${_kernel_src}
    )
  endforeach()
endforeach()
