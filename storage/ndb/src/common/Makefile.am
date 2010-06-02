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

SUBDIRS =	portlib debugger util logger transporter mgmcommon

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES =
libcommon_la_LIBADD = \
              transporter/libtransporter.la   \
              debugger/libtrace.la     \
              debugger/signaldata/libsignaldataprint.la \
              mgmcommon/libmgmsrvcommon.la \
              portlib/libportlib.la      \
              logger/liblogger.la       \
              util/libgeneral.la 

windoze-dsp:
