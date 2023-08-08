#!/bin/sh

set -e

# Run clang-format and add modified files
MODIFIED_UNSTAGED=$(git -C . diff --name-only)
MODIFIED_STAGED=$(git -C . diff --name-only --cached --diff-filter=d)

./clang-format.sh

git add ${MODIFIED_STAGED}

if [[ ${MODIFIED_UNSTAGED} != $(git -C . diff --name-only) ]]; then
  echo "WARNING: Non-staged files were reformatted. Please review and/or add" \
    "them"
fi


