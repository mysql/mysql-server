# Copyright (C) 2006 MySQL AB
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

SET(IBMDB2I_PLUGIN_DYNAMIC "ha_ibmdb2i")
CHECK_INCLUDE_FILES(qlgusr.h HAVE_PASE_ENVIRONMENT)
IF(HAVE_PASE_ENVIRONMENT)

INCLUDE_DIRECTORIES(
 ${CMAKE_SOURCE_DIR}/include 
 ${CMAKE_BINARY_DIR}/include
 ${CMAKE_SOURCE_DIR}/regex
 ${CMAKE_SOURCE_DIR}/sql
 ${CMAKE_CURRENT_SOURCE_DIR}
 /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0f.xpf/cur/cmvc/base.pgm/my.xpf/apis 
 /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0.xpf/bld/cmvc/base.pgm/lg.xpf 
 /afs/rchland.ibm.com/lande/shadow/dev2000/osxpf/v5r4m0.xpf/bld/cmvc/base.pgm/tq.xpf
)


SET (IBMDB2I_SOURCES ha_ibmdb2i.cc db2i_ileBridge.cc db2i_conversion.cc 
            db2i_blobCollection.cc db2i_file.cc db2i_charsetSupport.cc 
            db2i_collationSupport.cc db2i_errors.cc db2i_constraints.cc
            db2i_rir.cc db2i_sqlStatementStream.cc db2i_ioBuffers.cc db2i_myconv.cc)

SET(IBMDB2I_LIBS iconv)
MYSQL_STORAGE_ENGINE(IBMDB2I)

ENDIF(HAVE_PASE_ENVIRONMENT)
