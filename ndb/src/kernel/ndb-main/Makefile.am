
bin_PROGRAMS = ndb

ndb_SOURCES = Main.cpp SimBlockList.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am
INCLUDES += \
              -I../blocks/cmvmi \
              -I../blocks/dbacc \
              -I../blocks/dbdict \
              -I../blocks/dbdih \
              -I../blocks/dblqh \
              -I../blocks/dbtc \
              -I../blocks/dbtup \
              -I../blocks/ndbfs \
              -I../blocks/ndbcntr \
              -I../blocks/qmgr \
              -I../blocks/trix \
              -I../blocks/backup \
              -I../blocks/dbutil \
              -I../blocks/suma \
              -I../blocks/grep \
              -I../blocks/dbtux

LDADD +=  \
              $(top_srcdir)/ndb/src/mgmapi/libmgmapi.la \
              ../blocks/cmvmi/libcmvmi.a \
              ../blocks/dbacc/libdbacc.a \
              ../blocks/dbdict/libdbdict.a \
              ../blocks/dbdih/libdbdih.a \
              ../blocks/dblqh/libdblqh.a \
              ../blocks/dbtc/libdbtc.a \
              ../blocks/dbtup/libdbtup.a \
              ../blocks/ndbfs/libndbfs.a \
              ../blocks/ndbcntr/libndbcntr.a \
              ../blocks/qmgr/libqmgr.a \
              ../blocks/trix/libtrix.a \
              ../blocks/backup/libbackup.a \
              ../blocks/dbutil/libdbutil.a \
              ../blocks/suma/libsuma.a \
              ../blocks/grep/libgrep.a \
              ../blocks/dbtux/libdbtux.a \
              ../vm/libkernel.a	\
              ../error/liberror.a \
              $(top_srcdir)/ndb/src/common/transporter/libtransporter.la   \
              $(top_srcdir)/ndb/src/common/debugger/libtrace.la     \
              $(top_srcdir)/ndb/src/common/debugger/signaldata/libsignaldataprint.la \
              $(top_srcdir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
              $(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la      \
              $(top_srcdir)/ndb/src/common/logger/liblogger.la       \
              $(top_srcdir)/ndb/src/common/util/libgeneral.la 

# Don't update the files from bitkeeper
%::SCCS/s.%
