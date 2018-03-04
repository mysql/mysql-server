# Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.
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

SET(MYISAMMRG_SOURCES  myrg_close.c myrg_create.c myrg_delete.c myrg_extra.c myrg_info.c
				ha_myisammrg.cc
				myrg_locking.c myrg_open.c myrg_panic.c myrg_queue.c myrg_range.c
				myrg_rfirst.c myrg_rkey.c myrg_rlast.c myrg_rnext.c myrg_rnext_same.c
				myrg_rprev.c myrg_rrnd.c myrg_rsame.c myrg_static.c myrg_update.c
				myrg_write.c myrg_records.c)

MYSQL_ADD_PLUGIN(myisammrg ${MYISAMMRG_SOURCES} STORAGE_ENGINE MANDATORY)
