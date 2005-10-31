INCLUDES = -I../include -I../taocrypt/include -I../mySTL

noinst_LIBRARIES = libyassl.a
libyassl_a_SOURCES = buffer.cpp cert_wrapper.cpp crypto_wrapper.cpp \
	handshake.cpp lock.cpp log.cpp socket_wrapper.cpp ssl.cpp \
	template_instnt.cpp timer.cpp yassl_imp.cpp yassl_error.cpp yassl_int.cpp
EXTRA_DIST = ../include/*.hpp ../include/openssl/*.h
AM_CXXFLAGS = -DYASSL_PURE_C
