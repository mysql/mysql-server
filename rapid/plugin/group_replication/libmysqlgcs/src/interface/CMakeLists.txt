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

ADD_SUBDIRECTORY(xplatform)

ADD_LIBRARY(mysql_gcs_interface_static STATIC gcs_group_identifier.cc
                                              gcs_member_identifier.cc
                                              gcs_message.cc
                                              gcs_view.cc
                                              gcs_types.cc
                                              gcs_logging.cc)

ADD_DEPENDENCIES(mysql_gcs_interface_static mysql_gcs_xplatform)

TARGET_LINK_LIBRARIES(mysql_gcs_interface_static mysql_gcs_xplatform)

MERGE_STATIC_LIBS(mysql_gcs_interface mysql_gcs_interface
                  "mysql_gcs_interface_static;mysql_gcs_xplatform")
