#!/bin/bash

boost_version_ul="1_79_0" # underline
boost_version_d="1.79.0"  # dot
install_prefix=/usr/

echo "build boost ..."
cd "${third_party_dir}" || exit
if [ ! -f boost_${boost_version_ul}.tar.gz ]; then
  wget -c https://boostorg.jfrog.io/artifactory/main/release/${boost_version_d}/source/boost_${boost_version_ul}.tar.gz || exit
fi
tar -xvf boost_${boost_version_ul}.tar.gz
cd boost_${boost_version_ul}
./bootstrap.sh --prefix="${install_prefix}"
./b2
./b2 install
cd "${third_party_dir}" || exit
rm -rf boost_${boost_version_ul}