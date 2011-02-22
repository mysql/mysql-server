CXXFLAGS += -fimplicit-templates
pkgplugindir =  $(PLUGIN_DIR)
noinst_HEADERS = database.hpp hstcpsvr.hpp hstcpsvr_worker.hpp mysql_incl.hpp
pkgplugin_LTLIBRARIES = handlersocket.la
handlersocket_la_LDFLAGS = -module ../libhsclient/libhsclient.la
handlersocket_la_CFLAGS = $(MYSQL_INC) $(MYSQL_CFLAGS) $(AM_CFLAGS) \
	-I$(srcdir)/../libhsclient
handlersocket_la_CXXFLAGS = $(MYSQL_INC) $(MYSQL_CFLAGS) $(AM_CFLAGS) \
	-I$(srcdir)/../libhsclient
handlersocket_la_SOURCES = database.cpp handlersocket.cpp \
	hstcpsvr_worker.cpp hstcpsvr.cpp
