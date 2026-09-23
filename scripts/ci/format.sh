#!/usr/bin/env bash
# Copyright (c) 2026, Oracle and/or its affiliates.
# Format staged C/C++ using the repo's existing .clang-format.
# Run scripts/ci/format.sh with no args as a pre-commit hook; pass paths to format specific files.
set -euo pipefail
REPO_ROOT="$(git rev-parse --show-toplevel)"
CF="$(command -v clang-format-18 || command -v clang-format)"

if [ "$#" -gt 0 ]; then files="$*";
else files="$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cc|cpp|h|hpp)$' || true)"; fi

[ -z "${files// }" ] && { echo "format: nothing to do"; exit 0; }
for f in $files; do [ -f "$REPO_ROOT/$f" ] && "$CF" -i --style=file "$REPO_ROOT/$f"; done
git add $files 2>/dev/null || true
echo "format: applied .clang-format to changed C/C++ files"
