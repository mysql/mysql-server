SUBDIRS =	portlib debugger util logger transporter mgmcommon editline

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES =
libcommon_la_LIBADD = \
              transporter/libtransporter.la   \
              debugger/libtrace.la     \
              debugger/signaldata/libsignaldataprint.la \
              mgmcommon/libmgmsrvcommon.la \
              portlib/libportlib.la      \
              logger/liblogger.la       \
              util/libgeneral.la 
