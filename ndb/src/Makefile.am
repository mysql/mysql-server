SUBDIRS = common mgmapi ndbapi . kernel mgmsrv mgmclient cw

include $(top_srcdir)/ndb/config/common.mk.am

ndblib_LTLIBRARIES = libndbclient.la

libndbclient_la_SOURCES =

libndbclient_la_LIBADD =  \
              ndbapi/libndbapi.la \
              common/transporter/libtransporter.la   \
              common/debugger/libtrace.la     \
              common/debugger/signaldata/libsignaldataprint.la \
              mgmapi/libmgmapi.la \
              common/mgmcommon/libmgmsrvcommon.la \
              common/logger/liblogger.la       \
              common/portlib/libportlib.la      \
              common/util/libgeneral.la 
