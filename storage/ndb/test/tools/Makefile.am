# Copyright (C) 2004-2005 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

ndbtest_PROGRAMS = hugoLoad hugoFill hugoLockRecords hugoPkDelete hugoPkRead hugoPkReadRecord  hugoPkUpdate hugoScanRead hugoScanUpdate restart verify_index copy_tab create_index  ndb_cpcc listen_event eventlog rep_latency ndb_connect

# transproxy 

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
listen_event_SOURCES = listen.cpp
eventlog_SOURCES = log_listner.cpp
rep_latency_SOURCES = rep_latency.cpp
ndb_connect_SOURCES = connect.cpp

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapitest.mk.am

ndb_cpcc_LDADD = $(LDADD)
ndb_cpcc_LDFLAGS = -static

windoze-dsp:
