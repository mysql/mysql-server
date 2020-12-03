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

set -e

usage()
{
  echo
  echo "Computes test coverage by generating baseline.info and "\
       "combining coverage files from test_coverage directory."
  echo
  echo "Usage: ${0} --results-dir=<results_dir> --build-dir=<build_dir> "\
      " [--help]"
  echo "Options:"
  echo "  --results-dir  - Directory holding 'result.N' directories."
  echo "  --build-dir    - Directory where source code was built with "\
                           "coverage flags enabled."
  echo "  --help         - Displays help"
}

while [ "$1" ]; do
  case "$1" in
    --results-dir=*) results_dir=$(echo "${1}" | sed s/--results-dir=//);;
    --build-dir=*) build_dir=$(echo "${1}" | sed s/--build-dir=//);;
    --help) usage; exit 0;;
    *) echo "ERROR: Invalid option ${1}..." >&2; usage; exit 1;;
  esac
  shift
done

test_coverage_dir="${results_dir}/test_coverage"
if [ ! -d "${test_coverage_dir}" ]; then
  echo "Test coverage files not present to compute coverage" >&2
  exit 1
fi

resources_to_cleanup=''
trap "rm -rf ${resources_to_cleanup}" EXIT

baseline_info="${results_dir}/baseline.info"

if [ ! -f "${baseline_info}" ]; then
  lcov -c --initial --no-external -d "${build_dir}/storage/ndb" \
    -o "${baseline_info}"
fi

for coverage_file in "${test_coverage_dir}"/*; do
  lcov -a "${baseline_info}" -a "${coverage_file}" -o "${coverage_file}"

  # Filter out coverage from build files which will not be hit by any test.
  lcov --remove "${coverage_file}" "${build_dir}*" -o "${coverage_file}"

  # Extract coverage for NDB code only.
  lcov --extract "${coverage_file}" "*/storage/ndb/*" -o "${coverage_file}"
done

result=0
find "${test_coverage_dir}" -name "*.info" -exec echo "-a {}" \; | \
  xargs -r -x lcov -o "${results_dir}/coverage.info"
result="$?"

if [ "${result}" -ne 0 ]; then
  echo "Coverage report could not be generated: ${result}" >&2
  exit 1
fi
