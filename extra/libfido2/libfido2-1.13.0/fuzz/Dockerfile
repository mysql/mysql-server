# Copyright (c) 2019-2023 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

FROM alpine:latest
ENV CC=clang
ENV CXX=clang++
RUN apk -q update
RUN apk add build-base clang clang-analyzer cmake compiler-rt coreutils
RUN apk add eudev-dev git linux-headers llvm openssl-dev pcsc-lite-dev
RUN apk add sudo tar zlib-dev
RUN git clone --branch v0.10.1 --depth=1 https://github.com/PJK/libcbor
RUN git clone --depth=1 https://github.com/yubico/libfido2
WORKDIR /libfido2
RUN ./fuzz/build-coverage /libcbor /libfido2
