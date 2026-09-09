# Use FindDoxygen.cmake to generate documentation.

option(DOXYGEN_GENERATE_HTML  "Generate HTML"      ON)
option(DOXYGEN_GENERATE_MAN   "Generate man pages" OFF)
option(DOXYGEN_MAN_LINKS      "Generate man links" ON)
option(DOXYGEN_GENERATE_LATEX "Generate LaTeX"     OFF)

# If the case-insensitive value of the cmake option is one of
# "off, no, false" or 0, it is equal to false, otherwise true.
# And the values of the doxygen config does not exactly match it.
# So we need to convert the cmake option to a doxygen config.
macro(_convert_to_dx_cfg CMK_OPTION)
  if (${CMK_OPTION})
    set(${CMK_OPTION} YES)
  else()
    set(${CMK_OPTION} NO)
  endif()
endmacro()

macro(UseDoxygen)
  if (${CMAKE_VERSION} VERSION_LESS "3.9")
    # Old versions of cmake have poor support for Doxygen generation.
    message(FATAL_ERROR "Doxygen generation only enabled for cmake 3.9 and higher")
  else()
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
      set(DOXYGEN_PROJECT_NAME ${PROJECT_NAME})
      set(DOXYGEN_PROJECT_NUMBER ${EVENT_PACKAGE_VERSION})
      set(DOXYGEN_PROJECT_BRIEF "Event notification library")
      set(DOXYGEN_OUTPUT_DIRECTORY doxygen)
      set(DOXYGEN_STRIP_FROM_PATH include)
      set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
      set(DOXYGEN_OPTIMIZE_OUTPUT_FOR_C YES)
      set(DOXYGEN_SORT_BRIEF_DOCS YES)
      set(DOXYGEN_RECURSIVE NO)

      _convert_to_dx_cfg(DOXYGEN_GENERATE_HTML)
      _convert_to_dx_cfg(DOXYGEN_GENERATE_MAN)
      _convert_to_dx_cfg(DOXYGEN_MAN_LINKS)
      _convert_to_dx_cfg(DOXYGEN_GENERATE_LATEX)

      set(DOXYGEN_LATEX_CMD_NAME latex)
      set(DOXYGEN_PAPER_TYPE a4wide)
      set(DOXYGEN_PDF_HYPERLINKS NO)

      set(DOXYGEN_GENERATE_RTF NO)
      set(DOXYGEN_GENERATE_XML NO)
      set(DOXYGEN_GENERATE_CHI NO)

      set(DOXYGEN_PREDEFINED TAILQ_ENTRY
        RB_ENTRY
        EVENT_DEFINED_TQENTRY_
        EVENT_IN_DOXYGEN_
      )

      set(DOX_INPUT include/event2/buffer.h
        include/event2/buffer_compat.h
        include/event2/bufferevent.h
        include/event2/bufferevent_compat.h
        include/event2/bufferevent_ssl.h
        include/event2/dns.h
        include/event2/dns_compat.h
        include/event2/event.h
        include/event2/event_compat.h
        include/event2/http.h
        include/event2/http_compat.h
        include/event2/listener.h
        include/event2/rpc.h
        include/event2/rpc_compat.h
        include/event2/tag.h
        include/event2/tag_compat.h
        include/event2/thread.h
        include/event2/util.h
      )
      # Add 'doxygen' target
      doxygen_add_docs(doxygen
        ${DOX_INPUT}
        ALL
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating doxygen documentation for ${PROJECT_NAME}..."
      )

      # Use 'make clean' to remove the generated directory
      set_property(DIRECTORY
        PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
        "${PROJECT_BINARY_DIR}/${DOXYGEN_OUTPUT_DIRECTORY}"
      )

      # Install html into <prefix>/share/doc/<project>
      if ("${DOXYGEN_GENERATE_HTML}" STREQUAL "YES")
        install(DIRECTORY
          ${PROJECT_BINARY_DIR}/${DOXYGEN_OUTPUT_DIRECTORY}/html
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/${PROJECT_NAME}
          COMPONENT doc
        )
      endif()

      # Install manual into <prefix>/share/man/man3
      if ("${DOXYGEN_GENERATE_MAN}" STREQUAL "YES")
        install(DIRECTORY
          ${PROJECT_BINARY_DIR}/${DOXYGEN_OUTPUT_DIRECTORY}/man/man3
          DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man
          COMPONENT doc
        )
      endif()

    else(DOXYGEN_FOUND)
      message(FATAL_ERROR "Doxygen command not found, set EVENT__DOXYGEN to disable")
    endif (DOXYGEN_FOUND)
  endif()
endmacro()
