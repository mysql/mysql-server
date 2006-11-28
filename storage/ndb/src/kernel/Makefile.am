SUBDIRS = vm error blocks

include $(top_srcdir)/storage/ndb/config/common.mk.am

ndbbin_PROGRAMS = ndbd

ndbd_SOURCES = main.cpp SimBlockList.cpp

include $(top_srcdir)/storage/ndb/config/type_kernel.mk.am

INCLUDES += \
	-I$(srcdir)/blocks/cmvmi \
	-I$(srcdir)/blocks/dbacc \
	-I$(srcdir)/blocks/dbdict \
	-I$(srcdir)/blocks/dbdih \
	-I$(srcdir)/blocks/dblqh \
	-I$(srcdir)/blocks/dbtc \
	-I$(srcdir)/blocks/dbtup \
	-I$(srcdir)/blocks/ndbfs \
	-I$(srcdir)/blocks/ndbcntr \
	-I$(srcdir)/blocks/qmgr \
	-I$(srcdir)/blocks/trix \
	-I$(srcdir)/blocks/backup \
	-I$(srcdir)/blocks/dbutil \
	-I$(srcdir)/blocks/suma \
	-I$(srcdir)/blocks/dbtux \
	-I$(srcdir)/blocks

LDADD +=  \
              blocks/libblocks.a \
              vm/libkernel.a	\
              error/liberror.a \
              $(top_builddir)/storage/ndb/src/common/transporter/libtransporter.la   \
              $(top_builddir)/storage/ndb/src/common/debugger/libtrace.la     \
              $(top_builddir)/storage/ndb/src/common/debugger/signaldata/libsignaldataprint.la \
              $(top_builddir)/storage/ndb/src/common/logger/liblogger.la       \
              $(top_builddir)/storage/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
              $(top_builddir)/storage/ndb/src/mgmapi/libmgmapi.la \
              $(top_builddir)/storage/ndb/src/common/portlib/libportlib.la      \
              $(top_builddir)/storage/ndb/src/common/util/libgeneral.la  \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a @NDB_SCI_LIBS@

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: ndbd.dsp

storage/ndbd.dsp: Makefile \
               $(top_srcdir)/storage/ndb/config/win-prg.am \
               $(top_srcdir)/storage/ndb/config/win-name \
               $(top_srcdir)/storage/ndb/config/win-includes \
               $(top_srcdir)/storage/ndb/config/win-sources \
               $(top_srcdir)/storage/ndb/config/win-libraries
	cat $(top_srcdir)/storage/ndb/config/win-prg.am > $@
	@$(top_srcdir)/storage/ndb/config/win-name $@ $(ndbbin_PROGRAMS)
	@$(top_srcdir)/storage/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/storage/ndb/config/win-sources $@ $(ndbd_SOURCES)
	@$(top_srcdir)/storage/ndb/config/win-libraries $@ LINK $(LDADD)
