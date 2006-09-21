INCLUDES = -I../include -I../taocrypt/include -I../mySTL
bin_PROGRAMS       = testsuite
testsuite_SOURCES  = testsuite.cpp ../taocrypt/test/test.cpp \
	../examples/client/client.cpp ../examples/server/server.cpp \
	../examples/echoclient/echoclient.cpp \
	../examples/echoserver/echoserver.cpp
testsuite_CXXFLAGS = -DYASSL_PURE_C -DYASSL_PREFIX -DNO_MAIN_DRIVER
testsuite_LDADD    = $(top_builddir)/extra/yassl/src/libyassl.la \
                     $(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la
EXTRA_DIST = testsuite.dsp test.hpp input quit make.bat

# Don't update the files from bitkeeper
%::SCCS/s.%

