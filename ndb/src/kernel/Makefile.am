SUBDIRS = error blocks vm

include $(top_srcdir)/ndb/config/common.mk.am

ndbbin_PROGRAMS = ndbd

ndbd_SOURCES = main.cpp SimBlockList.cpp

include $(top_srcdir)/ndb/config/type_kernel.mk.am

INCLUDES += \
              -Iblocks/cmvmi \
              -Iblocks/dbacc \
              -Iblocks/dbdict \
              -Iblocks/dbdih \
              -Iblocks/dblqh \
              -Iblocks/dbtc \
              -Iblocks/dbtup \
              -Iblocks/ndbfs \
              -Iblocks/ndbcntr \
              -Iblocks/qmgr \
              -Iblocks/trix \
              -Iblocks/backup \
              -Iblocks/dbutil \
              -Iblocks/suma \
              -Iblocks/grep \
              -Iblocks/dbtux

LDADD +=  \
              blocks/cmvmi/libcmvmi.a \
              blocks/dbacc/libdbacc.a \
              blocks/dbdict/libdbdict.a \
              blocks/dbdih/libdbdih.a \
              blocks/dblqh/libdblqh.a \
              blocks/dbtc/libdbtc.a \
              blocks/dbtup/libdbtup.a \
              blocks/ndbfs/libndbfs.a \
              blocks/ndbcntr/libndbcntr.a \
              blocks/qmgr/libqmgr.a \
              blocks/trix/libtrix.a \
              blocks/backup/libbackup.a \
              blocks/dbutil/libdbutil.a \
              blocks/suma/libsuma.a \
              blocks/grep/libgrep.a \
              blocks/dbtux/libdbtux.a \
              vm/libkernel.a	\
              error/liberror.a \
              $(top_builddir)/ndb/src/common/transporter/libtransporter.la   \
              $(top_builddir)/ndb/src/common/debugger/libtrace.la     \
              $(top_builddir)/ndb/src/common/debugger/signaldata/libsignaldataprint.la \
              $(top_builddir)/ndb/src/common/logger/liblogger.la       \
              $(top_builddir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
              $(top_builddir)/ndb/src/mgmapi/libmgmapi.la \
              $(top_builddir)/ndb/src/common/portlib/libportlib.la      \
              $(top_builddir)/ndb/src/common/util/libgeneral.la  \
         $(top_builddir)/dbug/libdbug.a \
         $(top_builddir)/mysys/libmysys.a \
         $(top_builddir)/strings/libmystrings.a @NDB_SCI_LIBS@

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp: ndbd.dsp

ndbd.dsp: Makefile \
               $(top_srcdir)/ndb/config/win-prg.am \
               $(top_srcdir)/ndb/config/win-name \
               $(top_srcdir)/ndb/config/win-includes \
               $(top_srcdir)/ndb/config/win-sources \
               $(top_srcdir)/ndb/config/win-libraries
	cat $(top_srcdir)/ndb/config/win-prg.am > $@
	@$(top_srcdir)/ndb/config/win-name $@ $(ndbbin_PROGRAMS)
	@$(top_srcdir)/ndb/config/win-includes $@ $(INCLUDES)
	@$(top_srcdir)/ndb/config/win-sources $@ $(ndbd_SOURCES)
	@$(top_srcdir)/ndb/config/win-libraries $@ LINK $(LDADD)
