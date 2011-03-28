# Copyright (C) 2003, 2008 MySQL AB
#  All rights reserved. Use is subject to license terms.
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
