SUBDIRS = common mgmapi ndbapi kernel mgmsrv mgmclient cw

ndblib_LTLIBRARIES = libndbclient.la

libndbclient_la_LIBADD =  \
              $(top_srcdir)/ndb/src/ndbapi/libndbapi.la \
              $(top_srcdir)/ndb/src/common/transporter/libtransporter.la   \
              $(top_srcdir)/ndb/src/common/debugger/libtrace.la     \
              $(top_srcdir)/ndb/src/common/debugger/signaldata/libsignaldataprint.la \
              $(top_srcdir)/ndb/src/common/mgmcommon/libmgmsrvcommon.la \
              $(top_srcdir)/ndb/src/mgmapi/libmgmapi.la \
              $(top_srcdir)/ndb/src/common/logger/liblogger.la       \
              $(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la      \
              $(top_srcdir)/ndb/src/common/util/libgeneral.la 
