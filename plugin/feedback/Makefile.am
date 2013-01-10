pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex -I$(top_srcdir)/sql

EXTRA_LTLIBRARIES =	feedback.la libfeedback.la
pkgplugin_LTLIBRARIES =	@plugin_feedback_shared_target@
feedback_la_LDFLAGS =   -module -rpath $(pkgplugindir) -L$(top_builddir)/libservices -lmysqlservices
if HAVE_YASSL
feedback_la_LIBADD =     @yassl_libs@
endif
feedback_la_CXXFLAGS =  -shared -DMYSQL_DYNAMIC_PLUGIN
feedback_la_SOURCES =   feedback.cc utils.cc url_base.cc url_http.cc \
			sender_thread.cc

noinst_LTLIBRARIES =	@plugin_feedback_static_target@
libfeedback_la_SOURCES=	feedback.cc utils.cc url_base.cc url_http.cc \
			sender_thread.cc

noinst_HEADERS = 	feedback.h
EXTRA_DIST =		CMakeLists.txt plug.in

