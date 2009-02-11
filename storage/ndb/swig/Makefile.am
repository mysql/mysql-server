# Copyright (C) 2004-2006 MySQL AB
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

include $(top_srcdir)/storage/ndb/config/common.mk.am

nobase_swiginclude_HEADERS = \
mgmapi/mgmglobals.i \
mgmapi/ClusterState.i \
mgmapi/events.i \
mgmapi/listeners.i \
mgmapi/NdbLogEvent.i \
mgmapi/NdbLogEventManager.i \
mgmapi/NdbMgmFactory.i \
mgmapi/NdbMgm.i \
mgmapi/NdbMgmReply.i \
mgmapi/NodeState.i \
ndbapi/NdbError.i \
ndbapi/ndbglobals.i \
ndbapi/NdbIndexScanOperation.i \
ndbapi/NdbOperation.i \
ndbapi/NdbScanFilter.i \
ndbapi/NdbScanOperation.i \
ndbapi/NdbBlob.i \
ndbapi/NdbClusterConnection.i \
ndbapi/NdbDictionary.i \
ndbapi/NdbEventOperation.i \
ndbapi/NdbFactory.i \
ndbapi/Ndb.i \
ndbapi/NdbIndexOperation.i \
ndbapi/NdbRecAttr.i \
ndbapi/NdbTransaction.i \
globals.i

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`

# Don't update the files from bitkeeper
%::SCCS/s.%
