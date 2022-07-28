# Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

add_harness_plugin(routertestplugin_bad_one NO_INSTALL
  DESTINATION harness
  SOURCES bad_one.cc)
add_harness_plugin(routertestplugin_bad_two NO_INSTALL
  DESTINATION harness
  SOURCES bad_two.cc)
add_harness_plugin(routertestplugin_magic NO_INSTALL
  DESTINATION harness
  SOURCES magic.cc)
TARGET_INCLUDE_DIRECTORIES(routertestplugin_magic PRIVATE ../include)

add_harness_plugin(routertestplugin_example NO_INSTALL
  DESTINATION harness
  SOURCES example.cc
  REQUIRES routertestplugin_magic)

add_harness_plugin(routertestplugin_lifecycle NO_INSTALL
  DESTINATION harness
  SOURCES lifecycle.cc
)
TARGET_INCLUDE_DIRECTORIES(routertestplugin_lifecycle PRIVATE ../include)

add_harness_plugin(routertestplugin_lifecycle2 NO_INSTALL
  DESTINATION harness
  SOURCES lifecycle2.cc)
add_harness_plugin(routertestplugin_lifecycle3 NO_INSTALL
  DESTINATION harness
  SOURCES lifecycle3.cc)
