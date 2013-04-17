function(add_c_defines)
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS ${ARGN})
endfunction(add_c_defines)

## os name detection (threadpool-test.cc needs this)
if (CMAKE_SYSTEM_NAME MATCHES Darwin)
  add_c_defines(DARWIN=1 _DARWIN_C_SOURCE)
elseif (CMAKE_SYSTEM_NAME MATCHES Linux)
#  add_c_defines(__linux__=1)
endif ()

## preprocessor definitions we want everywhere
add_c_defines(
  _SVID_SOURCE
  _XOPEN_SOURCE=600
  _FILE_OFFSET_BITS=64
  _LARGEFILE64_SOURCE
  __STDC_FORMAT_MACROS
  __STDC_LIMIT_MACROS
  )

## add TOKU_PTHREAD_DEBUG for debug builds
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG TOKU_PTHREAD_DEBUG)

if (CMAKE_SYSTEM_NAME STREQUAL Darwin OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
  message(WARNING "Setting TOKU_ALLOW_DEPRECATED on Darwin and with clang.  TODO: remove this.")
  add_c_defines(TOKU_ALLOW_DEPRECATED)
endif ()

## coverage
option(USE_GCOV "Use gcov for test coverage." OFF)
if (USE_GCOV)
  if (NOT CMAKE_CXX_COMPILER_ID MATCHES GNU)
    message(FATAL_ERROR "Must use the GNU compiler to compile for test coverage.")
  endif ()
  find_program(COVERAGE_COMMAND NAMES gcov47 gcov)
endif (USE_GCOV)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

#####################################################
## Xcode needs to have this set manually.
## Other generators (makefiles) ignore this setting.
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

## adds a compiler flag if the compiler supports it
macro(set_cflags_if_supported_named flag flagname)
  check_c_compiler_flag("${flag}" HAVE_C_${flagname})
  if (HAVE_C_${flagname})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
  endif ()
  check_cxx_compiler_flag("${flag}" HAVE_CXX_${flagname})
  if (HAVE_CXX_${flagname})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
  endif ()
endmacro(set_cflags_if_supported_named)

## adds a compiler flag if the compiler supports it
macro(set_cflags_if_supported)
  foreach(flag ${ARGN})
    check_c_compiler_flag(${flag} HAVE_C_${flag})
    if (HAVE_C_${flag})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    endif ()
    check_cxx_compiler_flag(${flag} HAVE_CXX_${flag})
    if (HAVE_CXX_${flag})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
    endif ()
  endforeach(flag)
endmacro(set_cflags_if_supported)

## adds a linker flag if the compiler supports it
macro(set_ldflags_if_supported)
  foreach(flag ${ARGN})
    check_cxx_compiler_flag(${flag} HAVE_${flag})
    if (HAVE_${flag})
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${flag}")
    endif ()
  endforeach(flag)
endmacro(set_ldflags_if_supported)

## disable some warnings
set_cflags_if_supported(
  -Wno-missing-field-initializers
  -Wstrict-null-sentinel
  -Winit-self
  -Wswitch
  -Wtrampolines
  -Wlogical-op
  -Wmissing-format-attribute
  -Wno-error=missing-format-attribute
  -fno-rtti
  -fno-exceptions
  )

## Clang has stricter POD checks.  So, only enable this warning on our other builds (Linux + GCC)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
  set_cflags_if_supported(
    -Wpacked
    )
endif ()

## set_cflags_if_supported_named("-Weffc++" -Weffcpp)
set_ldflags_if_supported(
  -Wno-error=strict-overflow
  )

## set extra debugging flags and preprocessor definitions
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb -O0")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG FORTIFY_SOURCE=2)

## set extra release flags, we overwrite this because the default passes -DNDEBUG and we don't want that
if (CMAKE_CXX_COMPILER_ID STREQUAL Clang AND CMAKE_SYSTEM_NAME STREQUAL Darwin)
  set(CMAKE_C_FLAGS_RELEASE "-g -O4")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -O4")
else ()
  set(CMAKE_C_FLAGS_RELEASE "-g -O3")
  set(CMAKE_CXX_FLAGS_RELEASE "-g -O3")

  ## check how to do inter-procedural optimization
  check_c_compiler_flag(-flto HAVE_C_FLAG_FLTO)
  check_c_compiler_flag(-ipo HAVE_C_FLAG_IPO)
  if (HAVE_C_FLAG_FLTO)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto -fuse-linker-plugin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -fuse-linker-plugin")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g -fuse-linker-plugin")
  elseif (HAVE_C_FLAG_IPO)
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ip -ipo1")
  endif ()
  check_cxx_compiler_flag(-flto HAVE_CXX_FLAG_FLTO)
  check_cxx_compiler_flag(-ipo HAVE_CXX_FLAG_IPO)
  if (HAVE_CXX_FLAG_FLTO)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fuse-linker-plugin")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -fuse-linker-plugin")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -g -fuse-linker-plugin")
  elseif (HAVE_CXX_FLAG_IPO)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ip -ipo1")
  endif ()
endif ()

option(USE_VALGRIND "Do not pass NVALGRIND to the compiler, because valgrind will be run on the generated executables." ON)
if (NOT USE_VALGRIND)
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_RELEASE NVALGRIND=1)
endif ()

## We need to explicitly set this standard library for Xcode builds.
##add_definitions(-stdlib=libc++)

if (CMAKE_CXX_COMPILER_ID MATCHES Intel)
  set(CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
  ## make sure intel libs are linked statically
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-intel")

  ## disable some intel-specific warnings
  set(intel_warnings
    94     # allow arrays of length 0
    411    # allow const struct members without a constructor
    589    # do not complain about goto that skips initialization
    1292   # icc lies (it says it is "__GNUC__", but it doesn't handle the resulting macroexpansions from glibc 2.15.37 (which is designed for gcc 4.7, and appears in Fedora 17)
    2259   # do not complain about "non-pointer conversion from int to u_int8_t (and other small types) may lose significant bits".  this produces too many false positives
    11000  # do not remark about multi-file optimizations, single-file optimizations, and object temp files
    11001
    11006
    11003  # do not complain if some file was compiled without -ipo
    )
  string(REGEX REPLACE ";" "," intel_warning_string "${intel_warnings}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -diag-disable ${intel_warning_string}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -diag-disable ${intel_warning_string}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -diag-disable ${intel_warning_string}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -diag-disable ${intel_warning_string}")

  ## icc does -g differently
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -debug all")
  set(CMAKE_C_FLAGS "-Wcheck ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -debug all")
  set(CMAKE_CXX_FLAGS "-Wcheck ${CMAKE_CXX_FLAGS}")
else()
  set(CMAKE_C_FLAGS "-std=c99 ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")
  ## set gcc warnings
  set(CMAKE_C_FLAGS "-Wextra ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "-Wextra ${CMAKE_CXX_FLAGS}")
  set(WARN_CFLAGS
    -Wbad-function-cast
    -Wno-missing-noreturn
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wpointer-arith
    -Wmissing-format-attribute
    )
  if (WARN_ABOUT_PACKED)
    list(APPEND WARN_CFLAGS -Wpacked -Wno-error=packed)
  endif ()
  ## other flags to try:
  ##  -Wunsafe-loop-optimizations
  ##  -Wpointer-arith
  ##  -Wc++-compat
  ##  -Wc++11-compat
  ##  -Wwrite-strings
  ##  -Wzero-as-null-pointer-constant
  ##  -Wlogical-op
  ##  -Wvector-optimization-performance
  if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    message(WARNING "Disabling -Wcast-align and -Wshadow on osx.  TODO: fix casting and shadowed declarations and re-enable them.")
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    message(WARNING "Disabling -Wcast-align with clang.  TODO: fix casting and re-enable it.")
    list(APPEND WARN_CFLAGS -Wshadow)
  else ()
    list(APPEND WARN_CFLAGS -Wcast-align -Wshadow)
  endif ()
endif()

set_cflags_if_supported(${WARN_CFLAGS})
## always want these
set(CMAKE_C_FLAGS "-Wall -Werror ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS "-Wall -Werror ${CMAKE_CXX_FLAGS}")

function(add_space_separated_property type obj propname val)
  get_property(oldval ${type} ${obj} PROPERTY ${propname})
  if (oldval MATCHES NOTFOUND)
    set_property(${type} ${obj} PROPERTY ${propname} "${val}")
  else ()
    set_property(${type} ${obj} PROPERTY ${propname} "${val} ${oldval}")
  endif ()
endfunction(add_space_separated_property)

function(set_targets_need_intel_libs)
  if (CMAKE_CXX_COMPILER_ID STREQUAL Intel)
    foreach(tgt ${ARGN})
      target_link_libraries(${tgt} LINK_PUBLIC -Bstatic irc -Bdynamic stdc++)
    endforeach(tgt)
  endif ()
endfunction(set_targets_need_intel_libs)

## this function makes sure that the libraries passed to it get compiled
## with gcov-needed flags, we only add those flags to our libraries
## because we don't really care whether our tests get covered
function(maybe_add_gcov_to_libraries)
  if (USE_GCOV)
    foreach(lib ${ARGN})
      add_space_separated_property(TARGET ${lib} COMPILE_FLAGS --coverage)
      add_space_separated_property(TARGET ${lib} LINK_FLAGS --coverage)
      target_link_libraries(${lib} gcov)
    endforeach(lib)
  endif (USE_GCOV)
endfunction(maybe_add_gcov_to_libraries)

## adds -fvisibility=hidden -fPIE to compile phase
## adds -pie (or -Wl,-pie) to link phase
## good for binaries
function(add_common_options_to_binary_targets)
  foreach(tgt ${ARGN})
    if (CMAKE_C_COMPILER_ID STREQUAL Clang)
      add_space_separated_property(TARGET ${tgt} COMPILE_FLAGS "-fvisibility=hidden")
    else ()
      add_space_separated_property(TARGET ${tgt} COMPILE_FLAGS "-fvisibility=hidden -fPIE")
      add_space_separated_property(TARGET ${tgt} LINK_FLAGS "-fPIE -pie")
    endif ()
  endforeach(tgt)
endfunction(add_common_options_to_binary_targets)
