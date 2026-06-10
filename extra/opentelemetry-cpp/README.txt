# Copyright (c) 2022, 2026, Oracle and/or its affiliates.
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

HOWTO import a new opentelemetry-cpp package 

Using release 1.12.0 as an example, adjust to the proper release number.

1) Read the release notes

https://github.com/open-telemetry/opentelemetry-cpp/releases/tag/v1.12.0

If opentelemetry-cpp upgraded its own dependencies,
the following packages may need to be upgraded as well:

- extra/json
- extra/opentelemetry-proto

2) Download the .tar.gz 

https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.12.0.tar.gz

3) Unpack the .tar.gz in a sub directory

cd extra/opentelemetry-cpp
tar xvf opentelemetry-cpp-1.12.0.tar.gz

This should create a directory named extra/opentelemetry-cpp/opentelemetry-cpp-1.12.0

Code for a package MUST be in a dedicated sub directory.

4) Remove un necessary code

cd extra/opentelemetry-cpp/opentelemetry-cpp-1.12.0

rm -rf .github
rm -rf bazel
rm -rf buildscripts
rm -rf ci
rm -rf docker
rm -rf docs
rm -rf examples
rm -rf exporters/elasticsearch
rm -rf exporters/etw
rm -rf exporters/prometheus
rm -rf exporters/zipkin
rm -rf functional
rm -rf opentracing-shim
rm -rf third_party
rm -rf tools

5) Commit and push.

It is important to have a separate commit for the import alone,
for the git history.

At this point, the new code is imported, but not used yet.

6) Adjust CMake

In cmake/opentelemetry-cpp.cmake,
point the tag to the new release

SET(OPENTELEMETRY_CPP_TAG "opentelemetry-cpp-1.12.0" CACHE INTERNAL "")

Apply local patches if needed

7) Do a full build and test

In particular, make sure the code still builds in MYSQL_MAINTAINER_MODE,
and adjust warnings flags for third party code if needed.

See extra/opentelemetry-cpp/CMakeFiles.txt

8) Commit and push.

At this point, the new code is used,
and the old code is still in the repository.

9) Cleanup the previous release

cd extra/opentelemetry-cpp/
rm -rf opentelemetry-cpp-<old version>

10) Commit and push.

It is important to have a separate commit for the cleanup alone,
for the git history.
