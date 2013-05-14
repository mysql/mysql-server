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

EXTRA_DIST = CMakeLists.txt

noinst_LIBRARIES = libNDBT.a

libNDBT_a_SOURCES = \
	NDBT_ReturnCodes.cpp \
	NDBT_Error.cpp NDBT_Tables.cpp NDBT_ResultRow.cpp \
	NDBT_Test.cpp HugoCalculator.cpp \
	HugoOperations.cpp HugoTransactions.cpp \
	HugoAsynchTransactions.cpp UtilTransactions.cpp \
	NdbRestarter.cpp NdbRestarts.cpp NDBT_Output.cpp \
	NdbBackup.cpp  NdbConfig.cpp NDBT_Table.cpp \
	NdbSchemaCon.cpp NdbSchemaOp.cpp getarg.c AtrtClient.cpp \
	CpcClient.cpp NdbMixRestarter.cpp NDBT_Thread.cpp DbUtil.cpp \
	SocketInputStream2.cpp NDBT_Find.cpp \
	HugoQueryBuilder.cpp HugoQueries.cpp

INCLUDES_LOC = -I$(top_srcdir)/storage/ndb/src/common/mgmcommon -I$(top_srcdir)/storage/ndb/include/mgmcommon -I$(top_srcdir)/storage/ndb/include/kernel -I$(top_srcdir)/storage/ndb/src/mgmapi -I$(top_srcdir)/include -I$(top_srcdir)/storage/ndb/include/debugger

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapitest.mk.am

