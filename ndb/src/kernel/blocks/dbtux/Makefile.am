noinst_LIBRARIES = libdbtux.a

libdbtux_a_SOURCES = \
                        DbtuxGen.cpp \
			DbtuxMeta.cpp \
			DbtuxMaint.cpp \
			DbtuxNode.cpp \
			DbtuxTree.cpp \
			DbtuxScan.cpp \
			DbtuxSearch.cpp \
			DbtuxCmp.cpp \
			DbtuxDebug.cpp

INCLUDES_LOC = -I$(top_srcdir)/ndb/src/kernel/blocks/dbtup

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
