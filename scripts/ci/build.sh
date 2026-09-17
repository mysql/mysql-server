#!/usr/bin/env bash
# Copyright (c) 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

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
