INCLUDES = -I../include -I../taocrypt/include -I../mySTL
bin_PROGRAMS       = testsuite
testsuite_SOURCES  = testsuite.cpp ../taocrypt/test/test.cpp \
	../examples/client/client.cpp ../examples/server/server.cpp \
	../examples/echoclient/echoclient.cpp \
	../examples/echoserver/echoserver.cpp
testsuite_LDFLAGS  = -L../src/ -L../taocrypt/src $(PTHREAD_LDFLAGS)
testsuite_CXXFLAGS = -DYASSL_PURE_C -DNO_MAIN_DRIVER -Wall -Wno-unused $(PTHREAD_CFLAGS)
testsuite_LDADD    = -lyassl -ltaocrypt $(EXTRA_PTHREAD) $(EXTRA_SOCKET)
testsuite_DEPENDENCIES = ../src/libyassl.la ../taocrypt/src/libtaocrypt.la
EXTRA_DIST = testsuite.dsp test.hpp input quit make.bat
