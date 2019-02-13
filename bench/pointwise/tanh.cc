/*
 * Copyright 2018 Codeplay Software Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use these files except in compliance with the License.
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
#include "snn_fixture.h"

#include "src/backend/eigen_backend_provider.h"

#include "sycldnn/backend/eigen_backend.h"

#include "sycldnn/pointwise/direction.h"
#include "sycldnn/pointwise/operators.h"

#define TANH_BM_WITH_DIRECTION(N, DIRECTION)                           \
  POINTWISE_BENCHMARK("Tanh", OP##_##DIRECTION##_##N##_##EigenBackend, \
                      sycldnn::backend::EigenBackend, N,               \
                      sycldnn::pointwise::DIRECTION, sycldnn::pointwise::Tanh)

#define TANH_BENCHMARK(N)            \
  TANH_BM_WITH_DIRECTION(N, Forward) \
  TANH_BM_WITH_DIRECTION(N, Gradient)

/** Sizes used correspond to the sizes of inputs for relu layers in ResNet.
 * Where the resulting sizes are identical, they are skipped. The sizes
 * are kept the same for tanh for the sake of fair comparison.
 *
 * Channels | Width | Height |
 * ---------|-------|--------|
 *       64 |   112 |    112 | --> 802,816
 *       64 |    56 |     56 | --> 200,704
 *      128 |    28 |     28 | --> 100,352
 *      512 |    28 |     28 | --> 401,408
 *      256 |    14 |     14 | -->  50,176
 */
TANH_BENCHMARK(802816);
TANH_BENCHMARK(200704);
TANH_BENCHMARK(100352);
TANH_BENCHMARK(401408);
TANH_BENCHMARK(50176);
