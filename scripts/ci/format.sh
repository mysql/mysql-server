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

# Format staged C/C++ using the repo's existing .clang-format.
# Run scripts/ci/format.sh with no args as a pre-commit hook; pass paths to format specific files.
set -euo pipefail
REPO_ROOT="$(git rev-parse --show-toplevel)"
CF="$(command -v clang-format-15 || command -v clang-format)"

if [ "$#" -gt 0 ]; then files="$*";
else files="$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cc|cpp|h|hpp)$' || true)"; fi

[ -z "${files// }" ] && { echo "format: nothing to do"; exit 0; }
for f in $files; do [ -f "$REPO_ROOT/$f" ] && "$CF" -i --style=file "$REPO_ROOT/$f"; done
git add $files 2>/dev/null || true
echo "format: applied .clang-format to changed C/C++ files"
