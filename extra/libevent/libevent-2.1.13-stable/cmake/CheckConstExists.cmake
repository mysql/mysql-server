include(CheckCSourceCompiles)

macro(check_const_exists CONST FILES VARIABLE)
  if (NOT DEFINED ${VARIABLE})
    set(check_const_exists_source "")
    foreach(file ${FILES})
      set(check_const_exists_source
          "${check_const_exists_source}
          #include <${file}>")
    endforeach()
    set(check_const_exists_source
        "${check_const_exists_source}
        int main() { (void)${CONST}; return 0; }")

    check_c_source_compiles("${check_const_exists_source}" ${VARIABLE})

    if (${${VARIABLE}})
      set(${VARIABLE} 1 CACHE INTERNAL "Have const ${CONST}")
      message(STATUS "Looking for ${CONST} - found")
    else()
      set(${VARIABLE} 0 CACHE INTERNAL "Have const ${CONST}")
      message(STATUS "Looking for ${CONST} - not found")
    endif()
  endif()
endmacro(check_const_exists)
