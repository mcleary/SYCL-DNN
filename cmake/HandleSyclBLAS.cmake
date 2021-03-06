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

cmake_minimum_required(VERSION 3.3)

include(SNNHelpers)
snn_include_guard(HANDLE_SYCLBLAS)

if(NOT SNN_DOWNLOAD_SYCLBLAS)
  find_package(SyclBLAS)
endif()

if(NOT SyclBLAS_FOUND AND (SNN_DOWNLOAD_SYCLBLAS OR SNN_DOWNLOAD_MISSING_DEPS))
  include(ExternalProject)
  set(SyclBLAS_REPO "https://github.com/codeplaysoftware/sycl-blas.git" CACHE STRING
    "SyclBLAS git repository to clone"
  )
  set(SyclBLAS_GIT_TAG "c5325b8" CACHE STRING
    "Git tag, branch or commit to use for the SyclBLAS library"
  )
  set(SyclBLAS_SOURCE_DIR ${sycldnn_BINARY_DIR}/SyclBLAS-src)
  set(SyclBLAS_BINARY_DIR ${sycldnn_BINARY_DIR}/SyclBLAS-build)
  set(SyclBLAS_LIBNAME "")
  set(SyclBLAS_LIBRARIES "")
  set(SyclBLAS_BYPRODUCTS ${SyclBLAS_LIBRARIES})
  if(CMAKE_CROSSCOMPILING)
    set(cmake_toolchain
      "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    )
  endif()
  if(NOT TARGET SyclBLAS_download)
    ExternalProject_Add(SyclBLAS_download
      GIT_REPOSITORY    ${SyclBLAS_REPO}
      GIT_TAG           ${SyclBLAS_GIT_TAG}
      SOURCE_DIR        ${SyclBLAS_SOURCE_DIR}
      BINARY_DIR        ${SyclBLAS_BINARY_DIR}
      CMAKE_ARGS        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        -DBUILD_SHARED_LIBS=OFF
                        ${cmake_toolchain}
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     ""
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
      BUILD_BYPRODUCTS ${SyclBLAS_BYPRODUCTS}
    )
  endif()
  set(SyclBLAS_INCLUDE_DIR
    ${SyclBLAS_SOURCE_DIR}/include CACHE PATH
    "The SyclBLAS include directory" FORCE
  )
  set(SyclBLAS_SRC_DIR
    ${SyclBLAS_SOURCE_DIR}/src CACHE PATH
    "The SyclBLAS source directory" FORCE
  )
  set(SyclBLAS_VPTR_INCLUDE_DIR
    ${SyclBLAS_SOURCE_DIR}/external/computecpp-sdk/include CACHE PATH
    "The SyclBLAS virtual pointer include directory" FORCE
  )
  set(SyclBLAS_INCLUDE_DIRS ${SyclBLAS_INCLUDE_DIR} ${SyclBLAS_SRC_DIR} ${SyclBLAS_VPTR_INCLUDE_DIR})
  # Have to explicitly make the include directories to add it to the library
  # target. This will be filled with the headers at build time when the
  # library is downloaded.
  file(MAKE_DIRECTORY ${SyclBLAS_INCLUDE_DIR})
  file(MAKE_DIRECTORY ${SyclBLAS_SRC_DIR})
  file(MAKE_DIRECTORY ${SyclBLAS_VPTR_INCLUDE_DIR})

  find_package(SyclBLAS)
  add_dependencies(SyclBLAS::SyclBLAS SyclBLAS_download)
  mark_as_advanced(SyclBLAS_REPO SyclBLAS_GIT_TAG)
endif()

if(NOT SyclBLAS_FOUND)
  message(FATAL_ERROR
    "Could not find SyclBLAS, consider setting SNN_DOWNLOAD_MISSING_DEPS")
endif()
