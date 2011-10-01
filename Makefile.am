pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex -I$(top_srcdir)/sql

EXTRA_LTLIBRARIES =	feedback.la
pkgplugin_LTLIBRARIES =	@plugin_feedback_shared_target@
feedback_la_LDFLAGS =   -module -rpath $(pkgplugindir)
feedback_la_CXXFLAGS =  -shared -DMYSQL_DYNAMIC_PLUGIN
feedback_la_SOURCES =   feedback.cc utils.cc url_base.cc url_http.cc \
			sender_thread.cc

EXTRA_LIBRARIES =	libfeedback.a
noinst_LIBRARIES =	@plugin_feedback_static_target@
libfeedback_a_SOURCES=	feedback.cc utils.cc url_base.cc url_http.cc \
			sender_thread.cc

noinst_HEADERS = 	feedback.h
EXTRA_DIST =		CMakeLists.txt plug.in

