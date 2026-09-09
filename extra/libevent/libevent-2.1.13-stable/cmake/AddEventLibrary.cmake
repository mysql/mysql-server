include(CMakeParseArguments)

set(LIBEVENT_SHARED_LIBRARIES "")
set(LIBEVENT_STATIC_LIBRARIES "")

macro(set_event_shared_lib_flags LIB_NAME)
    set_target_properties("${LIB_NAME}_shared" PROPERTIES
        COMPILE_FLAGS ${ARGN})
    set_target_properties("${LIB_NAME}_shared" PROPERTIES
        LINK_FLAGS ${ARGN})
endmacro()

macro(generate_pkgconfig LIB_NAME)
    set(prefix      ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix ${CMAKE_INSTALL_PREFIX})
    set(libdir      ${CMAKE_INSTALL_PREFIX}/lib)
    set(includedir  ${CMAKE_INSTALL_PREFIX}/include)

    set(VERSION ${EVENT_ABI_LIBVERSION})

    set(LIBS         "")
    foreach (LIB ${LIB_PLATFORM})
        set(LIBS "${LIBS} -L${LIB}")
    endforeach()

    set(OPENSSL_LIBS "")
    foreach(LIB ${OPENSSL_LIBRARIES})
        set(OPENSSL_LIBS "${OPENSSL_LIBS} -L${LIB}")
    endforeach()

    configure_file("lib${LIB_NAME}.pc.in" "lib${LIB_NAME}.pc" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/lib${LIB_NAME}.pc"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig"
    )
endmacro()

# LIB_NAME maybe event_core, event_extra, event_openssl, event_pthreads or event.
# Targets whose LIB_NAME is not 'event' should be exported and installed.
macro(export_install_target TYPE LIB_NAME OUTER_INCLUDES)
    if("${LIB_NAME}" STREQUAL "event")
        install(TARGETS "${LIB_NAME}_${TYPE}"
            LIBRARY DESTINATION "lib" COMPONENT lib
            ARCHIVE DESTINATION "lib" COMPONENT lib
            RUNTIME DESTINATION "lib" COMPONENT lib
            COMPONENT dev
        )
    else()
        string(REPLACE "event_" "" PURE_NAME ${LIB_NAME})
        string(TOUPPER ${TYPE} UPPER_TYPE)
        list(APPEND LIBEVENT_${UPPER_TYPE}_LIBRARIES "${PURE_NAME}")
        set(OUTER_INCS)
        if (NOT "${OUTER_INCLUDES}" STREQUAL "NONE")
            set(OUTER_INCS ${OUTER_INCLUDES})
        endif()
        target_include_directories("${LIB_NAME}_${TYPE}"
            PUBLIC  "$<INSTALL_INTERFACE:include>"
                    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                    "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
                    ${OUTER_INCS}
        )
        set_target_properties("${LIB_NAME}_${TYPE}" PROPERTIES EXPORT_NAME ${PURE_NAME})
        export(TARGETS "${LIB_NAME}_${TYPE}"
            NAMESPACE ${PROJECT_NAME}::
            FILE "${PROJECT_BINARY_DIR}/LibeventTargets-${TYPE}.cmake"
            APPEND
        )
        install(TARGETS "${LIB_NAME}_${TYPE}"
            EXPORT LibeventTargets-${TYPE}
            LIBRARY DESTINATION "lib" COMPONENT lib
            ARCHIVE DESTINATION "lib" COMPONENT lib
            RUNTIME DESTINATION "lib" COMPONENT lib
            COMPONENT dev
        )
    endif()
endmacro()

# Global variables that it uses:
# - EVENT_ABI_LIBVERSION
# - EVENT_ABI_LIBVERSION_CURRENT
# - EVENT_ABI_LIBVERSION_REVISION
# - EVENT_ABI_LIBVERSION_AGE
# - EVENT_PACKAGE_RELEASE
# - CMAKE_THREAD_LIBS_INIT LIB_PLATFORM
# - OPENSSL_LIBRARIES
# - EVENT_SHARED_FLAGS
# - EVENT_LIBRARY_STATIC
# - EVENT_LIBRARY_SHARED
#
# Exported variables:
# - LIBEVENT_SHARED_LIBRARIES
# - LIBEVENT_STATIC_LIBRARIES
macro(add_event_library LIB_NAME)
    cmake_parse_arguments(LIB
        "" # Options
        "VERSION" # One val
        "SOURCES;LIBRARIES;INNER_LIBRARIES;OUTER_INCLUDES" # Multi val
        ${ARGN}
    )

    if ("${LIB_OUTER_INCLUDES}" STREQUAL "")
        set(LIB_OUTER_INCLUDES NONE)
    endif()
    set(ADD_EVENT_LIBRARY_INTERFACE)

    if (${EVENT_LIBRARY_STATIC})
        add_library("${LIB_NAME}_static" STATIC ${LIB_SOURCES})
        set_target_properties("${LIB_NAME}_static" PROPERTIES
            OUTPUT_NAME "${LIB_NAME}"
            CLEAN_DIRECT_OUTPUT 1)

        if(LIB_INNER_LIBRARIES)
            set(INNER_LIBRARIES "${LIB_INNER_LIBRARIES}_static")
        endif()
        target_link_libraries("${LIB_NAME}_static"
            ${CMAKE_THREAD_LIBS_INIT}
            ${LIB_PLATFORM}
            ${INNER_LIBRARIES}
            ${LIB_LIBRARIES})

        export_install_target(static "${LIB_NAME}" "${LIB_OUTER_INCLUDES}")

        set(ADD_EVENT_LIBRARY_INTERFACE "${LIB_NAME}_static")
    endif()

    if (${EVENT_LIBRARY_SHARED})
        add_library("${LIB_NAME}_shared" SHARED ${LIB_SOURCES})

        if(LIB_INNER_LIBRARIES)
            set(INNER_LIBRARIES "${LIB_INNER_LIBRARIES}_shared")
        endif()
        target_link_libraries("${LIB_NAME}_shared"
            ${CMAKE_THREAD_LIBS_INIT}
            ${LIB_PLATFORM}
            ${INNER_LIBRARIES}
            ${LIB_LIBRARIES})

        if (EVENT_SHARED_FLAGS)
            set_event_shared_lib_flags("${LIB_NAME}" "${EVENT_SHARED_FLAGS}")
        endif()

        if (WIN32)
            set_target_properties(
                "${LIB_NAME}_shared" PROPERTIES
                OUTPUT_NAME "${LIB_NAME}"
                SOVERSION ${EVENT_ABI_LIBVERSION})
        elseif (APPLE)
            math(EXPR COMPATIBILITY_VERSION "${EVENT_ABI_LIBVERSION_CURRENT}+1")
            math(EXPR CURRENT_MINUS_AGE "${EVENT_ABI_LIBVERSION_CURRENT}-${EVENT_ABI_LIBVERSION_AGE}")
            set_target_properties(
                "${LIB_NAME}_shared" PROPERTIES
                OUTPUT_NAME "${LIB_NAME}-${EVENT_PACKAGE_RELEASE}.${CURRENT_MINUS_AGE}"
                INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
                LINK_FLAGS "-compatibility_version ${COMPATIBILITY_VERSION} -current_version ${COMPATIBILITY_VERSION}.${EVENT_ABI_LIBVERSION_REVISION}")
        else()
            math(EXPR CURRENT_MINUS_AGE "${EVENT_ABI_LIBVERSION_CURRENT}-${EVENT_ABI_LIBVERSION_AGE}")
            set_target_properties(
                "${LIB_NAME}_shared" PROPERTIES
                OUTPUT_NAME "${LIB_NAME}-${EVENT_PACKAGE_RELEASE}"
                VERSION "${CURRENT_MINUS_AGE}.${EVENT_ABI_LIBVERSION_AGE}.${EVENT_ABI_LIBVERSION_REVISION}"
                SOVERSION "${CURRENT_MINUS_AGE}"
                INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
        endif()

        if (NOT WIN32)
            set(LIB_LINK_NAME
                "${CMAKE_SHARED_LIBRARY_PREFIX}${LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")

            add_custom_command(TARGET ${LIB_NAME}_shared
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink
                    "$<TARGET_FILE_NAME:${LIB_NAME}_shared>"
                    "${LIB_LINK_NAME}"
                WORKING_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
        endif()

        export_install_target(shared "${LIB_NAME}" "${LIB_OUTER_INCLUDES}")

        set(ADD_EVENT_LIBRARY_INTERFACE "${LIB_NAME}_shared")

        if (NOT WIN32)
            install(FILES
                "$<TARGET_FILE_DIR:${LIB_NAME}_shared>/${LIB_LINK_NAME}"
                DESTINATION "lib"
                COMPONENT lib)
        endif()
    endif()

    add_library(${LIB_NAME} INTERFACE)
    target_link_libraries(${LIB_NAME} INTERFACE ${ADD_EVENT_LIBRARY_INTERFACE})

    generate_pkgconfig("${LIB_NAME}")
endmacro()
