#!/usr/bin/env bash
# Copyright (c) 2026, Oracle and/or its affiliates.
# Install/pin the build toolchain so a local build matches what reviewers'
# automation sees. Tested on Ubuntu 24.04.
set -euo pipefail

SUDO=""; [ "$(id -u)" -ne 0 ] && SUDO="sudo"

$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build pkg-config bison \
  libssl-dev libncurses-dev libldap2-dev libsasl2-dev libcurl4-openssl-dev libtirpc-dev \
  ccache git curl

echo "Toolchain ready. Boost is fetched on first configure via -DDOWNLOAD_BOOST=1."
echo "Next: scripts/ci/build.sh debug"
