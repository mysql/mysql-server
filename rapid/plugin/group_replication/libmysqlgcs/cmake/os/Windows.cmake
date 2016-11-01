# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

# Exclude unused headers and avoid messed up includes
# regarding winsock.h and winsock2.h
# See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms737629%28v=vs.85%29.aspx
ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN)

# When NOMINMMSVC defines micros of min and max which causes compile warning
# when use numeric_limits<T>::max()
ADD_DEFINITIONS(-DNOMINMAX)

