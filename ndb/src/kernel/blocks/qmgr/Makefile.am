noinst_LIBRARIES = libqmgr.a

libqmgr_a_SOURCES = \
	QmgrInit.cpp	\
	QmgrMain.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libqmgr.dsp

libqmgr.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libqmgr_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ $(LDADD)
