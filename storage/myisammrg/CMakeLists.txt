# Copyright (c) 2006, 2022, Oracle and/or its affiliates.
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

SET(MYISAMMRG_SOURCES
  ha_myisammrg.cc
  myrg_close.cc
  myrg_create.cc
  myrg_delete.cc
  myrg_extra.cc
  myrg_info.cc
  myrg_locking.cc
  myrg_open.cc
  myrg_panic.cc
  myrg_queue.cc myrg_range.cc
  myrg_records.cc
  myrg_rfirst.cc
  myrg_rkey.cc
  myrg_rlast.cc
  myrg_rnext.cc
  myrg_rnext_same.cc
  myrg_rprev.cc
  myrg_rrnd.cc
  myrg_static.cc
  myrg_update.cc
  myrg_write.cc
  )

# MyISAM relies heavily on memset/memcpy of structs, including non-POD ones.
# As MyISAM is not maintained anymore, it is not worth fixing these,
# so we turn off the warning instead.
IF(MY_COMPILER_IS_GNU)
  ADD_COMPILE_FLAGS(${MYISAMMRG_SOURCES} COMPILE_FLAGS "-Wno-class-memaccess")
ENDIF()

MYSQL_ADD_PLUGIN(myisammrg ${MYISAMMRG_SOURCES} STORAGE_ENGINE MANDATORY)
