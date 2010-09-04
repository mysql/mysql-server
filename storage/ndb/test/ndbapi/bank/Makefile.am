# Copyright (C) 2004 MySQL AB
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

ndbtest_PROGRAMS = testBank bankSumAccounts bankValidateAllGLs bankMakeGL bankTransactionMaker bankCreator bankTimer

noinst_LIBRARIES = libbank.a

libbank_a_SOURCES = Bank.cpp BankLoad.cpp Bank.hpp

testBank_SOURCES = testBank.cpp
bankSumAccounts_SOURCES = bankSumAccounts.cpp
bankValidateAllGLs_SOURCES = bankValidateAllGLs.cpp 
bankMakeGL_SOURCES = bankMakeGL.cpp 
bankTransactionMaker_SOURCES = bankTransactionMaker.cpp 
bankCreator_SOURCES = bankCreator.cpp  
bankTimer_SOURCES = bankTimer.cpp

LDADD_LOC = $(noinst_LIBRARIES)

include $(top_srcdir)/storage/ndb/config/common.mk.am
include $(top_srcdir)/storage/ndb/config/type_ndbapitest.mk.am

windoze-dsp:
