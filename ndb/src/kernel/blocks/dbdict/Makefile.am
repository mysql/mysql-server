noinst_LIBRARIES = libdbdict.a
EXTRA_PROGRAMS = printSchemaFile

libdbdict_a_SOURCES = Dbdict.cpp

printSchemaFile_SOURCES = printSchemaFile.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

LDADD +=  \
         $(top_builddir)/ndb/src/common/util/libgeneral.la \
         $(top_builddir)/ndb/src/common/portlib/libportlib.la \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: libdbdict.dsp

libdbdict.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-lib.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-lib.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(noinst_LIBRARIES)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(libdbdict_a_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LIB $(LDADD)
