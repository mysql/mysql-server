# Copyright (c) 2023, Oracle and/or its affiliates.
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

INCLUDE(libutils)

DISABLE_MISSING_PROFILE_WARNING()

SET(BACKTRACE_VERSION sha9ae4f4a)
SET(BACKTRACE_SOURCES
  ${BACKTRACE_VERSION}/backtrace.c
  ${BACKTRACE_VERSION}/dwarf.c
  ${BACKTRACE_VERSION}/elf.c
  ${BACKTRACE_VERSION}/fileline.c
  ${BACKTRACE_VERSION}/mmap.c
  ${BACKTRACE_VERSION}/mmapio.c
  ${BACKTRACE_VERSION}/posix.c
  ${BACKTRACE_VERSION}/print.c
  ${BACKTRACE_VERSION}/simple.c
  ${BACKTRACE_VERSION}/sort.c
  ${BACKTRACE_VERSION}/state.c
)
SET(BACKTRACE_DEFINITIONS
  BACKTRACE_ELF_SIZE=64
  BACKTRACE_XCOFF_SIZE=unused
  HAVE_ATOMIC_FUNCTIONS=1
  HAVE_CLOCK_GETTIME=1
  HAVE_DECL_GETPAGESIZE=1
  HAVE_DECL_STRNLEN=1
  HAVE_DECL__PGMPTR=0
  HAVE_DLFCN_H=1
  HAVE_DL_ITERATE_PHDR=1
  HAVE_FCNTL=1
  HAVE_GETIPINFO=1
  HAVE_INTTYPES_H=1
  HAVE_LINK_H=1
  HAVE_LSTAT=1
  HAVE_MEMORY_H=1
  HAVE_READLINK=1
  HAVE_STDINT_H=1
  HAVE_STDLIB_H=1
  HAVE_STRINGS_H=1
  HAVE_STRING_H=1
  HAVE_SYNC_FUNCTIONS=1
  HAVE_SYS_MMAN_H=1
  HAVE_SYS_STAT_H=1
  HAVE_SYS_TYPES_H=1
  HAVE_UNISTD_H=1
)
IF(FREEBSD)
  LIST(APPEND BACKTRACE_DEFINITIONS
    HAVE_KERN_PROC=1
    HAVE_KERN_PROC_ARGS=1
    )
ENDIF()

# Ensure no tail-call optimization to stabilize the number of reported frames
ADD_COMPILE_FLAGS(src/stacktrace.cc COMPILE_FLAGS -fno-optimize-sibling-calls)

ADD_CONVENIENCE_LIBRARY(backtrace ${BACKTRACE_SOURCES} src/stacktrace.cc
  INCLUDE_DIRECTORIES PRIVATE include/
  COMPILE_OPTIONS PRIVATE -funwind-tables
  COMPILE_DEFINITIONS PRIVATE ${BACKTRACE_DEFINITIONS}
)
TARGET_COMPILE_DEFINITIONS(backtrace INTERFACE HAVE_EXT_BACKTRACE)
TARGET_COMPILE_DEFINITIONS(backtrace_objlib INTERFACE HAVE_EXT_BACKTRACE)

TARGET_INCLUDE_DIRECTORIES(backtrace INTERFACE include/)
