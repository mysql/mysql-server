
# Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

# This file includes FreeBSD specific options and quirks, related to system checks

# Should not be needed any more, but kept for easy resurrection if needed
#   #Legacy option, maybe not needed anymore , taken as is from autotools build
#   ADD_DEFINITIONS(-DNET_RETRY_COUNT=1000000)

# The below was used for really old versions of FreeBSD, roughly: before 5.1.9
# ADD_DEFINITIONS(-DHAVE_BROKEN_REALPATH)
