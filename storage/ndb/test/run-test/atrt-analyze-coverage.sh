#!/bin/bash

# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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
  echo "Analyzes coverage files gathered from each test host."
  echo
  echo "Usage: ${0} --results-dir=<results_dir> --build-dir=<build_dir> "\
      "--coverage-tool=lcov|fastcov --test-case-no=<test_case_no> [--help]"
  echo "Options:"
  echo "  --results-dir   - Directory holding 'result.N' directories."
  echo "  --build-dir     - Directory where source code was built with "\
                            "coverage flags enabled."
  echo "  --test-case-no  - Optional, used only if test coverage is "\
                           "computed per test case."
  echo "  --coverage-tool - Tool that will be used for coverage analysis."
  echo "  --help          - Displays help"
}

while [ "${1}" ]; do
  case "${1}" in
    --results-dir=*) results_dir=$(echo "${1}" | sed s/--results-dir=//);;
    --build-dir=*) build_dir=$(echo "${1}" | sed s/--build-dir=//);;
    --test-case-no=*) test_case_no=$(echo "${1}" | sed s/--test-case-no=//);;
    --coverage-tool=*) coverage_tool=$(echo "${1}" | sed s/--coverage-tool=//);
        case "${coverage_tool}" in
          lcov);;
          fastcov);;
          *) echo "Coverage tool ${coverage_tool} not supported."\
              "lcov will be used..."; coverage_tool="lcov";
        esac;;
    --help) usage; exit 0;;
    *) echo "ERROR: Invalid option ${1}..." >&2; usage; exit 1;;
  esac
  shift
done

gcno_files=$(find "${build_dir}" -name "*.gcno" | wc -l)
if [ "${gcno_files}" -eq 0 ]; then
  echo "Gcno files are not present in build directory, "\
        "coverage cannot be computed." >&2
  exit 1
fi

test_coverage_dir="${results_dir}/test_coverage"
mkdir -p "${test_coverage_dir}"

resources_to_cleanup=''
trap 'rm -rf ${resources_to_cleanup}' EXIT

cd "${results_dir}/coverage_result/"

for host_dir in */; do
  host_dir="${host_dir%%/}"
  resources_to_cleanup+="${results_base_dir}/coverage_result/${host_dir} "

  find "${host_dir}" -name '*.gcda' -printf '%P\n' | \
    sed -e 's/[.]gcda$/.gcno/' > "${host_dir}/gcno-files"
  rsync -am --files-from="${host_dir}/gcno-files" "${build_dir}" "${host_dir}"

  if  [ "${coverage_tool}" = "lcov" ]; then
    lcov -c -d "${host_dir}" -o "${host_dir}.info"
  elif [ "${coverage_tool}" = "fastcov" ]; then
    ## --lcov generates coverage files that are compatible with lcov tool
    fastcov -d "${host_dir}" --lcov -o "${host_dir}.info"
  fi
done

coverage_file=""
if [ -n "${test_case_no}" ]; then
  coverage_file="test_coverage.${test_case_no}.info"
else
  coverage_file="test_coverage.suite.info"
fi

result=0
# Combine host.info files
if [ "${coverage_tool}" = "lcov" ]; then
  find . -name "*.info" -exec echo "-a {}" \; |
    xargs -r -x lcov -o "${test_coverage_dir}/${coverage_file}"
  result="$?"
elif [ "${coverage_tool}" = "fastcov" ]; then
  find . -name "*.info" -exec echo "{}" \; | xargs -r -x fastcov --lcov \
    -o "${test_coverage_dir}/${coverage_file}" -C
  result="$?"
fi

if [ "${result}" -ne 0 ]; then
  echo "Coverage Analysis Failed: ${result}" >&2
  exit 1
fi
