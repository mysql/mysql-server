
noinst_LTLIBRARIES = libtransporter.la

libtransporter_la_SOURCES = \
	Transporter.cpp \
	SendBuffer.cpp   \
	TCP_Transporter.cpp \
	TransporterRegistry.cpp \
        Packer.cpp

EXTRA_libtransporter_la_SOURCES = SHM_Transporter.cpp SHM_Transporter.unix.cpp SCI_Transporter.cpp

libtransporter_la_LIBADD = @ndb_transporter_opt_objs@
libtransporter_la_DEPENDENCIES = @ndb_transporter_opt_objs@

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/kernel -I$(top_srcdir)/ndb/include/transporter @NDB_SCI_INCLUDES@

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
