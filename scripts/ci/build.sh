#!/usr/bin/env bash
# Copyright (c) 2026, Oracle and/or its affiliates.
# One-command configure + build. Usage: scripts/ci/build.sh [debug|release] [extra cmake args]
# Keeps Boost out-of-tree and cached so re-clones don't re-download it.
set -euo pipefail

BUILD_TYPE="${1:-debug}"; shift || true
case "$BUILD_TYPE" in
  debug)   CMAKE_BT=Debug ;;
  release) CMAKE_BT=RelWithDebInfo ;;
  *) echo "usage: build.sh [debug|release] [extra cmake args]"; exit 2 ;;
esac

REPO_ROOT="$(git rev-parse --show-toplevel)"
BOOST_DIR="${MYSQL_BOOST_DIR:-$HOME/.cache/mysql-boost}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build}"
mkdir -p "$BOOST_DIR"

export CCACHE_DIR="${CCACHE_DIR:-$HOME/.cache/ccache}"

cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$CMAKE_BT" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DDOWNLOAD_BOOST=1 -DWITH_BOOST="$BOOST_DIR" -DDOWNLOAD_BOOST_TIMEOUT=600 \
  -DWITH_UNIT_TESTS=ON \
  "$@"

cmake --build "$BUILD_DIR" -j "$(nproc)"
echo "Built into $BUILD_DIR"
