configure_file(toku_config.h.in toku_config.h)
add_custom_target(generate_config_h DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h")

# detect when we are being built as a subproject
if (NOT DEFINED MYSQL_PROJECT_NAME_DOCSTRING)
  install(
    FILES toku_list.h toku_os.h
    DESTINATION include
    COMPONENT tokukv_headers
    )
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h"
    DESTINATION include
    COMPONENT tokukv_headers
    )
endif ()