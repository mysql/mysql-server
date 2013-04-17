configure_file(config.h.in config.h)
add_custom_target(generate_config_h DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/config.h")

install(
  FILES toku_list.h toku_os.h
  DESTINATION include
  )
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/config.h"
  DESTINATION include
  )