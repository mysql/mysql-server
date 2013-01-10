INCLUDES = -I$(srcdir)/../include -I$(srcdir)/../taocrypt/include -I$(srcdir)/../taocrypt/mySTL

noinst_LTLIBRARIES = libyassl.la
libyassl_la_SOURCES = buffer.cpp cert_wrapper.cpp crypto_wrapper.cpp \
	handshake.cpp lock.cpp log.cpp socket_wrapper.cpp ssl.cpp \
	template_instnt.cpp timer.cpp yassl_imp.cpp yassl_error.cpp yassl_int.cpp
EXTRA_DIST = $(wildcard ../include/*.hpp) $(wildcard ../include/openssl/*.h)
AM_CXXFLAGS = -DYASSL_PURE_C -DYASSL_PREFIX @yassl_thread_cxxflags@ \
	@SYMBOL_VISIBILITY@ @SYMBOL_VISIBILITY_INLINES@
