
ndbtest_PROGRAMS = hugoCalculator hugoLoad hugoFill hugoLockRecords hugoPkDelete hugoPkRead hugoPkReadRecord  hugoPkUpdate hugoScanRead hugoScanUpdate restart verify_index copy_tab create_index  ndb_cpcc

# transproxy 

hugoCalculator_SOURCES = hugoCalculator.cpp
hugoFill_SOURCES = hugoFill.cpp
hugoLoad_SOURCES = hugoLoad.cpp
hugoLockRecords_SOURCES = hugoLockRecords.cpp
hugoPkDelete_SOURCES = hugoPkDelete.cpp
hugoPkRead_SOURCES = hugoPkRead.cpp
hugoPkReadRecord_SOURCES = hugoPkReadRecord.cpp
hugoPkUpdate_SOURCES = hugoPkUpdate.cpp
hugoScanRead_SOURCES = hugoScanRead.cpp
hugoScanUpdate_SOURCES = hugoScanUpdate.cpp
restart_SOURCES = restart.cpp
# transproxy_SOURCES = transproxy.cpp
verify_index_SOURCES = verify_index.cpp
copy_tab_SOURCES = copy_tab.cpp
create_index_SOURCES = create_index.cpp
ndb_cpcc_SOURCES = cpcc.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

ndb_cpcc_LDADD = $(LDADD)

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp:
