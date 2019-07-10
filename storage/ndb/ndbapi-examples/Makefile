# Copyright (C) 2003, 2008 MySQL AB
#  All rights reserved. Use is subject to license terms.
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

BIN_DIRS := 	ndbapi_simple \
	 	ndbapi_async \
	 	ndbapi_async1 \
		ndbapi_retries \
		ndbapi_simple_index \
		ndbapi_s_i_ndbrecord \
		ndbapi_event \
		ndbapi_scan \
		ndbapi_blob \
		ndbapi_blob_ndbrecord \
		mgmapi_logevent \
		ndbapi_simple_dual \
		mgmapi_logevent2 \
		ndbapi_recattr_vs_record

bins: $(patsubst %, _bins_%, $(BIN_DIRS))

$(patsubst %, _bins_%, $(BIN_DIRS)) :
	$(MAKE) -C $(patsubst _bins_%, %, $@) $(OPTS)

libs:

clean:
	for f in ${BIN_DIRS}; do \
		$(MAKE) -C $$f $@;\
	done

clean_dep: clean
cleanall: clean
tidy: clean
distclean: clean
