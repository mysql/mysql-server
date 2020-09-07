#!/bin/bash

# Copyright (c) 2020 Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Input parameters:
#  * RESULTS_BASE_DIR = Directory holding "result.N" directories.
#  * BUILD_DIR = Directory where source code was built.
#  * TEST_CASE = Test case number is used to identify info files created
#     for particular test case.
#
# Copies ".gcno" files into coverage_result/hostDir that holds ".gcda" files
# after atrt-gather-result.sh is run with --coverage parameter. lcov is tool
# is run to obtain ".info" files for each host. ".info" files from each host is
# combined to obtain test coverage info which will be stored in
# "test_coverage" directory as test_coverage_<test_number>.info.

set -e

if [ $# -lt 3 ]; then
  echo "Usage: ${0} results-base-dir build-dir test-case-number" >&2
  exit 1
fi

RESULTS_BASE_DIR="${1}"
BUILD_DIR="${2}"
TEST_CASE="${3}"
shift 3

GCNO_FILES=$(find "${BUILD_DIR}" -name "*.gcno" | wc -l)
if [ "${GCNO_FILES}" -eq 0 ]; then
  echo "Gcno files are not present in build directory, coverage cannot" \
       "be computed." >&2
  exit 1
fi

TEST_COVERAGE_DIR="${RESULTS_BASE_DIR}/test_coverage"
mkdir -p "${TEST_COVERAGE_DIR}"

RESOURCES_TO_CLEANUP=''
trap 'rm -rf ${RESOURCES_TO_CLEANUP}' EXIT

cd "${RESULTS_BASE_DIR}/coverage_result/"

for host_dir in */; do
  host_dir="${host_dir%%/}"
  RESOURCES_TO_CLEANUP+="${RESULTS_BASE_DIR}/coverage_result/${host_dir} "

  find "${host_dir}" -name '*.gcda' -printf '%P\n' | \
    sed -e 's/[.]gcda$/.gcno/' > "${host_dir}/gcno-files"

  rsync -am --files-from="${host_dir}/gcno-files" "${BUILD_DIR}" "${host_dir}"
  lcov -c -d "${host_dir}" -o "${host_dir}.info"
done

# Combine host.info files
find . -name "*.info" -exec echo "-a {}" \; | \
  xargs -r -x lcov -o "${TEST_COVERAGE_DIR}/test_coverage.${TEST_CASE}.info"
RESULT="$?"

if [ "${RESULT}" -ne 0 ]; then
  echo "Coverage Analysis Failed: ${RESULT}" >&2
  exit 1
fi
