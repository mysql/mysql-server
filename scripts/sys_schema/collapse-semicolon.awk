<<<<<<< HEAD
#  Copyright (c) 2019, 2022, Oracle and/or its affiliates.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
=======
#  Copyright (c) 2019, 2023, Oracle and/or its affiliates.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License, version 2.0,
#  as published by the Free Software Foundation.
#
#  This program is also distributed with certain software (including
#  but not limited to OpenSSL) that is licensed under separate terms,
#  as designated in a particular file or component or in included license
#  documentation.  The authors of MySQL hereby grant you an additional
#  permission to link the program and your derivative works with the
#  separately licensed software that they have included with MySQL.
>>>>>>> pr/231
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
<<<<<<< HEAD
#  GNU General Public License for more details.
=======
#  GNU General Public License, version 2.0, for more details.
>>>>>>> pr/231
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

# USAGE
# awk -f collapse-semicolon.awk file.sql
#
# This script:
# - collapses lines until a terminating ';' is found
#
# As a result, multi line statements terminated by a ';'
# are printed into a single line statement.
#

BEGIN {ORS=" ";}
/;$/ {ORS="\n"; print; print ""; ORS=" "; next;}
{ print; }

END {}

