#!/usr/bin/env bash

# Usage: ./clang-format.sh <extra arguments>

DIRS="src test examples demo"
SOURCES=$(find ${DIRS} -name "*.c")
SOURCES+=" $(find ${DIRS} -name "*.h")"
SOURCES+=" $(find ${DIRS} -name "*.cpp")"

# TravisCI workaround to use new clang-format while avoiding painful aliasing
# into the subshell
if which clang-format-8; then
    clang-format-8 $@ -style=file -i ${SOURCES}
else
    clang-format $@ -style=file -i ${SOURCES}
fi

