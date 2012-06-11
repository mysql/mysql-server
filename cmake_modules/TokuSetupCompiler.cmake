## set c99 dialect
add_definitions("-std=c99")

function(add_c_defines)
  set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS ${ARGN})
endfunction(add_c_defines)

## os name detection (threadpool-test.c needs this)
if (CMAKE_SYSTEM_NAME MATCHES Darwin)
  add_c_defines(DARWIN=1 _DARWIN_C_SOURCE)
elseif (CMAKE_SYSTEM_NAME MATCHES Linux)
  add_c_defines(__linux__=1)
endif ()

## preprocessor definitions we want everywhere
add_c_defines(
  _SVID_SOURCE
  _XOPEN_SOURCE=600
  _FILE_OFFSET_BITS=64
  _LARGEFILE64_SOURCE
  )

if (CMAKE_SYSTEM_NAME MATCHES Darwin)
  message(WARNING "Setting TOKU_ALLOW_DEPRECATED on Darwin.  TODO: remove this.")
  add_c_defines(TOKU_ALLOW_DEPRECATED)
endif ()

## coverage
option(USE_GCOV "Use gcov for test coverage." OFF)
if (USE_GCOV)
  if (NOT CMAKE_C_COMPILER_ID MATCHES GNU)
    message(FATAL_ERROR "Must use the GNU compiler to compile for test coverage.")
  endif ()
endif (USE_GCOV)

## this function makes sure that the libraries passed to it get compiled
## with gcov-needed flags, we only add those flags to our libraries
## because we don't really care whether our tests get covered
function(maybe_add_gcov_to_libraries)
  if (USE_GCOV)
    foreach(lib ${ARGN})
      get_target_property(lib_compile_flags ${lib} COMPILE_FLAGS)
      if (lib_compile_flags MATCHES lib_compile_flags-NOTFOUND)
        set(lib_compile_flags "")
      endif ()
      get_target_property(lib_link_flags ${lib} LINK_FLAGS)
      if (lib_link_flags MATCHES lib_link_flags-NOTFOUND)
        set(lib_link_flags "")
      endif ()
      set_target_properties(${lib} PROPERTIES
        COMPILE_FLAGS "${lib_compile_flags} --coverage"
        LINK_FLAGS "${lib_link_flags} --coverage"
        )
      target_link_libraries(${lib} gcov)
    endforeach(lib)
  endif (USE_GCOV)
endfunction(maybe_add_gcov_to_libraries)

include(CheckCCompilerFlag)

## adds a compiler flag if the compiler supports it
macro(set_cflags_if_supported)
  foreach(flag ${ARGN})
    check_c_compiler_flag(${flag} HAVE_${flag})
    if (HAVE_${flag})
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    endif ()
  endforeach(flag)
endmacro(set_cflags_if_supported)

## disable some warnings
set_cflags_if_supported(
  -Wno-self-assign
  -Wno-missing-field-initializers
  -Wno-maybe-uninitialized
  )

## set extra debugging flags and preprocessor definitions
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb -O0")
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS_DEBUG FORTIFY_SOURCE=2)

## set extra release flags, we overwrite this because the default passes -DNDEBUG and we don't want that
set(CMAKE_C_FLAGS_RELEASE "-g3 -ggdb -O3")

## check how to do inter-procedural optimization
check_c_compiler_flag(-flto HAVE_CC_FLAG_FLTO)
check_c_compiler_flag(-ipo HAVE_CC_FLAG_IPO)

## add inter-procedural optimization flags
if (HAVE_CC_FLAG_FLTO)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
elseif (HAVE_CC_FLAG_IPO)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ip -ipo1")
endif ()

if (CMAKE_C_COMPILER_ID MATCHES Intel)
  ## make sure intel libs are linked statically
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-intel")

  ## disable some intel-specific warnings
  set(intel_warnings
    94     # allow arrays of length 0
    589    # do not complain about goto that skips initialization
    2259   # do not complain about "non-pointer conversion from int to u_int8_t (and other small types) may lose significant bits".  this produces too many false positives
    11000  # do not remark about multi-file optimizations, single-file optimizations, and object temp files
    11001
    11006
    11003  # do not complain if some file was compiled without -ipo
    )
  string(REGEX REPLACE ";" "," intel_warning_string "${intel_warnings}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -diag-disable ${intel_warning_string}")

  ## icc does -g differently
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -debug all")

  ## set icc warnings
  set(CMAKE_C_FLAGS "-Wcheck ${CMAKE_C_FLAGS}")
else()
  ## set gcc warnings
  set(CMAKE_C_FLAGS "-Wextra ${CMAKE_C_FLAGS}")
  set(WARN_CFLAGS
    -Wbad-function-cast
    -Wno-missing-noreturn
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wpointer-arith
    -Wmissing-format-attribute
    )
  if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    message(WARNING "Disabling -Wcast-align and -Wshadow on osx.  TODO: fix casting and shadowed declarations and re-enable them.")
  else ()
    list(APPEND WARN_CFLAGS -Wcast-align -Wshadow)
  endif ()
endif()

set_cflags_if_supported(${WARN_CFLAGS})
## always want these
set(CMAKE_C_FLAGS "-Wall -Werror ${CMAKE_C_FLAGS}")

## function for adding -fvisibility=hidden to targets
function(set_targets_visibility_hidden)
  if (NOT CMAKE_C_COMPILER_ID STREQUAL "Intel")
    foreach(target ${ARGN})
      get_target_property(flags ${target} COMPILE_FLAGS)
      set_target_properties(${target} PROPERTIES
        COMPILE_FLAGS "${COMPILE_FLAGS} -fvisibility=hidden")
    endforeach(target)
  endif ()
endfunction(set_targets_visibility_hidden)
