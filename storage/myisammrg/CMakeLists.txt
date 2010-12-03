# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

SET(MYISAMMRG_SOURCES  myrg_close.c myrg_create.c myrg_delete.c myrg_extra.c myrg_info.c
				ha_myisammrg.cc
				myrg_locking.c myrg_open.c myrg_panic.c myrg_queue.c myrg_range.c
				myrg_rfirst.c myrg_rkey.c myrg_rlast.c myrg_rnext.c myrg_rnext_same.c
				myrg_rprev.c myrg_rrnd.c myrg_rsame.c myrg_static.c myrg_update.c
				myrg_write.c myrg_records.c)

MYSQL_ADD_PLUGIN(myisammrg ${MYISAMMRG_SOURCES} STORAGE_ENGINE MANDATORY RECOMPILE_FOR_EMBEDDED)
