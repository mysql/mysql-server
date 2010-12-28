# Copyright (C) 2004, 2005 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test/ndb

test_SCRIPTS = ndbcluster

noinst_HEADERS = ndbcluster.sh

dist_test_DATA = ndb_config_2_node.ini

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''ndb_port''@!$(ndb_port)!g' \
	  -e 's!@''ndbbindir''@!$(ndbbindir)!g' \
	  -e 's!@''ndbtoolsdir''@!$(ndbtoolsdir)!g' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
