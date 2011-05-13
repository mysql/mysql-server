CXXFLAGS += -fimplicit-templates
instdir = $(includedir)/handlersocket
# TODO: these headers should be in dena/
inst_HEADERS = allocator.hpp config.hpp mutex.hpp string_util.hpp \
	auto_addrinfo.hpp escape.hpp socket.hpp thread.hpp auto_file.hpp \
	fatal.hpp string_buffer.hpp util.hpp auto_ptrcontainer.hpp \
	hstcpcli.hpp string_ref.hpp 
lib_LTLIBRARIES = libhsclient.la
libhsclient_la_SOURCES = config.cpp escape.cpp fatal.cpp hstcpcli.cpp \
	socket.cpp string_util.cpp
libhsclient_la_CFLAGS = $(AM_CFLAGS) 
libhsclient_la_CXXFLAGS = $(AM_CXXFLAGS) 
