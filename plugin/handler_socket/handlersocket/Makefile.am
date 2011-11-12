pkgplugindir =  $(PLUGIN_DIR)
CXXFLAGS += -fimplicit-templates
noinst_HEADERS = database.hpp hstcpsvr.hpp hstcpsvr_worker.hpp mysql_incl.hpp
pkgplugin_LTLIBRARIES = handlersocket.la
handlersocket_la_LDFLAGS = -module ../libhsclient/libhsclient.la -L$(top_builddir)/libservices -lmysqlservices
handlersocket_la_CXXFLAGS = $(MYSQL_INC) $(MYSQL_CFLAGS) $(AM_CXXFLAGS) -I$(srcdir)/../libhsclient
handlersocket_la_SOURCES = database.cpp handlersocket.cpp \
	hstcpsvr_worker.cpp hstcpsvr.cpp
