# Copyright (C) 2007 MySQL AB
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

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)

ADD_LIBRARY(ndbbank STATIC
            Bank.cpp BankLoad.cpp)

ADD_EXECUTABLE(testBank testBank.cpp)
TARGET_LINK_LIBRARIES(testBank ndbbank)
ADD_EXECUTABLE(bankSumAccounts bankSumAccounts.cpp)
TARGET_LINK_LIBRARIES(bankSumAccounts ndbbank)
ADD_EXECUTABLE(bankValidateAllGLs bankValidateAllGLs.cpp)
TARGET_LINK_LIBRARIES(bankValidateAllGLs ndbbank)
ADD_EXECUTABLE(bankMakeGL bankMakeGL.cpp)
TARGET_LINK_LIBRARIES(bankMakeGL ndbbank)
ADD_EXECUTABLE(bankTransactionMaker bankTransactionMaker.cpp)
TARGET_LINK_LIBRARIES(bankTransactionMaker ndbbank)
ADD_EXECUTABLE(bankCreator bankCreator.cpp)
TARGET_LINK_LIBRARIES(bankCreator ndbbank)
ADD_EXECUTABLE(bankTimer bankTimer.cpp)
TARGET_LINK_LIBRARIES(bankTimer ndbbank)
