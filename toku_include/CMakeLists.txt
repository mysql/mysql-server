configure_file(toku_config.h.in toku_config.h)
add_custom_target(generate_config_h DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h")

install(
  FILES toku_list.h toku_os.h
  DESTINATION include
  )
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/toku_config.h"
  DESTINATION include
  )
