#!/bin/bash -eu
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

mkdir build
cd build
# We disable libcbor's default sanitizers since we'll be configuring them ourselves via CFLAGS.
cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_INSTALL_PREFIX="$WORK" -D CBOR_CUSTOM_ALLOC=ON -D SANITIZE=OFF ..
make "-j$(nproc)"
make install

$CXX $CXXFLAGS -std=c++11 "-I$WORK/include" \
    ../oss-fuzz/cbor_load_fuzzer.cc -o "$OUT/cbor_load_fuzzer" \
    $LIB_FUZZING_ENGINE src/libcbor.a

