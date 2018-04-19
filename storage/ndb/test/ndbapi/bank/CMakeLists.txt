# Copyright (C) 2008 MySQL AB
# Use is subject to license terms
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

INCLUDE(${CMAKE_SOURCE_DIR}/storage/ndb/config/type_ndbapitest.cmake)

ADD_CONVENIENCE_LIBRARY(ndbbank
            Bank.cpp BankLoad.cpp)

ADD_EXECUTABLE(testBank testBank.cpp)
ADD_EXECUTABLE(bankSumAccounts bankSumAccounts.cpp)
ADD_EXECUTABLE(bankValidateAllGLs bankValidateAllGLs.cpp)
ADD_EXECUTABLE(bankMakeGL bankMakeGL.cpp)
ADD_EXECUTABLE(bankTransactionMaker bankTransactionMaker.cpp)
ADD_EXECUTABLE(bankCreator bankCreator.cpp)
ADD_EXECUTABLE(bankTimer bankTimer.cpp)

SET(BINS
    testBank
    bankSumAccounts
    bankValidateAllGLs
    bankMakeGL
    bankTransactionMaker
    bankCreator
    bankTimer)

FOREACH(B ${BINS})
  TARGET_LINK_LIBRARIES(${B} ndbbank)
  TARGET_LINK_LIBRARIES(${B} ndbNDBT)
  TARGET_LINK_LIBRARIES(${B} ndbclient_so)
ENDFOREACH()

INSTALL(TARGETS ${BINS} DESTINATION bin)
