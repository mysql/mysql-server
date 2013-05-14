# Copyright (c) 2004-2008 MySQL AB
# 

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

SUBDIRS = bank
EXTRA_DIST = CMakeLists.txt

ndbtest_PROGRAMS = \
testSpj \
flexBench \
drop_all_tabs \
create_all_tabs \
create_tab reorg_tab \
flexAsynch \
flexBench \
flexHammer \
flexTT \
testBackup \
testBasic \
testBasicAsynch \
testAsynchMultiwait \
testBlobs \
testDataBuffers \
testDict \
testIndex \
testLimits \
testMgm \
testMgmd \
testSingleUserMode \
testNativeDefault \
testNdbApi \
testNodeRestart \
testUpgrade \
testOIBasic \
testOperations \
testRestartGci \
testScan \
testInterpreter \
testScanFilter \
testScanInterpreter \
testScanPerf \
testSystemRestart \
testTimeout \
testTransactions \
testDeadlock \
test_event ndbapi_slow_select testReadPerf testLcp \
testPartitioning \
testBitfield \
DbCreate DbAsyncGenerator \
testSRBank \
test_event_merge \
testIndexStat \
ndbapi_50compat0 \
ndbapi_50compat1 \
testNDBT \
testReconnect \
testNdbinfo \
NdbRepStress \
msa \
testRedo

EXTRA_PROGRAMS = \
 test_event \
 test_event_merge \
 test_event_multi_table
#flexTimedAsynch
#testBlobs
#flex_bench_mysql

create_all_tabs_SOURCES = create_all_tabs.cpp
create_tab_SOURCES = create_tab.cpp
drop_all_tabs_SOURCES = drop_all_tabs.cpp
flexAsynch_SOURCES = flexAsynch.cpp
flexBench_SOURCES = flexBench.cpp
flexHammer_SOURCES = flexHammer.cpp
flexTT_SOURCES = flexTT.cpp
#flexTimedAsynch_SOURCES = flexTimedAsynch.cpp
#flex_bench_mysql_SOURCES = flex_bench_mysql.cpp
testBackup_SOURCES = testBackup.cpp
testBasic_SOURCES = testBasic.cpp
testSpj_SOURCES = testSpj.cpp
testBasicAsynch_SOURCES = testBasicAsynch.cpp
testAsynchMultiwait_SOURCES = testAsynchMultiwait.cpp
testBlobs_SOURCES = testBlobs.cpp
testDataBuffers_SOURCES = testDataBuffers.cpp
testDict_SOURCES = testDict.cpp
testIndex_SOURCES = testIndex.cpp
testLimits_SOURCES = testLimits.cpp
testMgm_SOURCES = testMgm.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/Config.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/ConfigInfo.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/InitConfigFileParser.cpp \
	$(top_srcdir)/storage/ndb/src/kernel/vm/mt_thr_config.cpp
testMgm_CXXFLAGS = -I$(top_srcdir)/storage/ndb/include/debugger
testMgmd_SOURCES = testMgmd.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/Config.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/ConfigInfo.cpp \
	$(top_srcdir)/storage/ndb/src/mgmsrv/InitConfigFileParser.cpp \
	$(top_srcdir)/storage/ndb/src/kernel/vm/mt_thr_config.cpp
testMgmd_LDFLAGS = -static
testMgmd_CXXFLAGS = -I$(top_srcdir)/storage/ndb/include/debugger
testSingleUserMode_SOURCES = testSingleUserMode.cpp
testNativeDefault_SOURCES = testNativeDefault.cpp
testNdbApi_SOURCES = testNdbApi.cpp
testNdbApi_CXXFLAGS = -I$(top_srcdir)/storage/ndb/src/ndbapi \
	-I$(top_srcdir)/storage/ndb/include/transporter
testNodeRestart_SOURCES = testNodeRestart.cpp
testUpgrade_SOURCES = testUpgrade.cpp
testUpgrade_LDADD = $(LDADD) $(top_srcdir)/libmysql_r/libmysqlclient_r.la
testOIBasic_SOURCES = testOIBasic.cpp
testOperations_SOURCES = testOperations.cpp
testRestartGci_SOURCES = testRestartGci.cpp
testScan_SOURCES = testScan.cpp ScanFunctions.hpp
testInterpreter_SOURCES = testInterpreter.cpp
testScanFilter_SOURCES = testScanFilter.cpp
testScanInterpreter_SOURCES = testScanInterpreter.cpp ScanFilter.hpp ScanInterpretTest.hpp 
testScanPerf_SOURCES = testScanPerf.cpp
testSystemRestart_SOURCES = testSystemRestart.cpp
testTimeout_SOURCES = testTimeout.cpp
testTransactions_SOURCES = testTransactions.cpp
testDeadlock_SOURCES = testDeadlock.cpp
test_event_SOURCES = test_event.cpp
ndbapi_slow_select_SOURCES = slow_select.cpp
testReadPerf_SOURCES = testReadPerf.cpp
testLcp_SOURCES = testLcp.cpp
testPartitioning_SOURCES = testPartitioning.cpp
testNDBT_SOURCES = testNDBT.cpp
testNDBT_LDADD = $(LDADD) $(top_srcdir)/libmysql_r/libmysqlclient_r.la
testReconnect_SOURCES = testReconnect.cpp
testReconnect_LDADD = $(LDADD) $(top_srcdir)/libmysql_r/libmysqlclient_r.la
testNdbinfo_SOURCES = testNdbinfo.cpp
testBitfield_SOURCES = testBitfield.cpp
NdbRepStress_SOURCES = acrt/NdbRepStress.cpp
DbCreate_SOURCES = bench/mainPopulate.cpp bench/dbPopulate.cpp bench/userInterface.cpp bench/dbPopulate.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp
DbAsyncGenerator_SOURCES = bench/mainAsyncGenerator.cpp bench/asyncGenerator.cpp bench/ndb_async2.cpp bench/dbGenerator.h bench/macros.h bench/userInterface.h bench/testData.h bench/testDefinitions.h bench/ndb_schema.hpp bench/ndb_error.hpp
testSRBank_SOURCES = testSRBank.cpp
test_event_merge_SOURCES = test_event_merge.cpp
test_event_multi_table_SOURCES = test_event_multi_table.cpp
testIndexStat_SOURCES = testIndexStat.cpp
reorg_tab_SOURCES = reorg_tab.cpp
msa_SOURCES = msa.cpp
testRedo_SOURCES = testRedo.cpp

ndbapi_50compat0_CPPFLAGS = -DNDBAPI_50_COMPAT
ndbapi_50compat0_SOURCES = ndbapi_50compat0.cpp
ndbapi_50compat0_LDADD = $(LDADD) $(top_srcdir)/libmysql/libmysqlclient.la

ndbapi_50compat1_CPPFLAGS = -DNDBAPI_50_COMPAT
ndbapi_50compat1_SOURCES = ndbapi_50compat1.cpp
ndbapi_50compat1_LDADD = $(LDADD) $(top_srcdir)/libmysql/libmysqlclient.la

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/include/kernel

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapitest.mk.am

testBackup_LDADD = $(LDADD) bank/libbank.a
testSRBank_LDADD = bank/libbank.a $(LDADD) 
NdbRepStress_LDADD = $(LDADD) $(top_builddir)/libmysql_r/libmysqlclient_r.la

