#!/usr/bin/env bash

# Usage: ./clang-format.sh <extra arguments>

DIRS="src test examples"
SOURCES=$(find ${DIRS} -name "*.c")
SOURCES+=" $(find ${DIRS} -name "*.h")"
SOURCES+=" $(find ${DIRS} -name "*.cpp")"

clang-format $@ -style=file -i ${SOURCES}
