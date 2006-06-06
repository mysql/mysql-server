ADD_DEFINITIONS("-DWIN32 -D_LIB -DYASSL_PREFIX")

INCLUDE_DIRECTORIES(include taocrypt/include mySTL)
ADD_LIBRARY(yassl src/buffer.cpp src/cert_wrapper.cpp src/crypto_wrapper.cpp src/handshake.cpp src/lock.cpp 
				src/log.cpp src/socket_wrapper.cpp src/ssl.cpp src/timer.cpp src/yassl_error.cpp 
				src/yassl_imp.cpp src/yassl_int.cpp)
