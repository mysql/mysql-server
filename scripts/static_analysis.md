<!---
Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.
//
This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.
//
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
-->

# Static code analysis (SCA) of MySQL source code

## Overview

Script scripts/static_analysis.py is a wrapper around different static
code analysis tools allowing you to easily check your code for possible
issues.
There are two supported modes of work:
 - check a single commit
 - check entire source code repository (or its sub-tree)

Currently, only clang-tidy tool is supported, other tools (such as cppcheck)
may get supported in the future.

## Dependencies

Script uses the following external tools:
 - git binary
 - clang-tidy binary
 - clang-tidy-diff.py script
 - compile_commands.json file (see "Prerequisites" below)

clang-tidy-diff.py script is used when checking a single commit,
because it allows us to only check the modified lines of the patch.

## Running the script

### Prerequisites

To enable clang-tidy static code analysis, you need to configure the build
to generate compile_commands.json file.

Example commands:
>  CC=clang CXX=clang++ cmake <path_to_src>
> -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DWITH_SYSTEM_LIBS=1
> -DWITH_ZLIB=bundled -DWITH_FIDO=bundled -DWITH_PROTOBUF=bundled
> -DWITH_ZSTD=bundled -DWITH_EDITLINE=bundled -DWITH_LZ4=bundled   
>  make -j$(nproc) clang_tidy_prerequisites

### Script invocation

Script must be invoked from a working directory being within the source tree
or else we may not be able to detect some parameters.
Script writes the analysis results to the standard output, so you may need
to redirect the output to file for permanent storage.
Warnings, errors, progress info or debug output are being written
to the standard error stream.

Some usage examples are given below.

#### Scan entire tree (using 4 jobs):
>  python3 ./scripts/static_analysis.py -j 4 --tree --path ./bld > results.txt

#### Scan entire tree using custom version of clang-tidy binary:
>  python3 ./scripts/static_analysis.py --tree --clang-tidy=/usr/bin/clang-tidy > results.txt

#### Scan entire tree with build path outside of source tree:
>  python3 ./scripts/static_analysis.py --tree --path ../../bld > results.txt

#### Scan part of the whole tree:
>  python3 ./scripts/static_analysis.py --tree --scan-root=/work/mysql/sql > results.txt

#### Scan single commit (no commit hash assumed HEAD):
>  python3 ./scripts/static_analysis.py --commit > results.txt    
>  python3 ./scripts/static_analysis.py --commit HEAD~2 > results.txt

#### Scan commit using custom path of clang-tidy-diff.py script:
>  python3 ./scripts/static_analysis.py --commit HEAD~2
> --clang-tidy-diff=/usr/share/clang/clang-tidy-diff.py > results.txt

### Script alternatives

After satisfying the prerequisites, instead of the script you can also
possibly run the clang-tidy manually with (assuming being run from within
the build folder).

#### Scan entire tree:
>  /opt/llvm-17.0.1/bin/run-clang-tidy -clang-tidy=/opt/llvm-17.0.1/bin/clang-tidy
> -j $(nproc) -quiet -p .  > results.txt

#### Scan single commit (HEAD):
>  git diff HEAD~ -U0  -- '*.cc' '*.cpp' '*.c++' '*.cxx' '*.c' '*.cl' '*.h' '*.hpp' ':!extra'
> | python3 clang-tidy-diff.py -timeout 600  -path .  -j=4 -p1 -extra-arg='-ferror-limit=0'

## Usage info

For detailed list of supported parameters, run:
>  python3 ./scripts/static_analysis.py --help
