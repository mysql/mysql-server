# Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.
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

#------------------------------------------------------------------------------

  SRC	= ./src/cpp

.PHONY:	all help dep dbg opt prf clean mostlclean clobber distclean \
	run.ndbapi run.crund
all:	dbg

help:
	@echo "dep		-- build dependencies"
	@echo "all		-- build all targets with default flags"
	@echo "dbg		-- build all targets with debug flags"
	@echo "opt		-- build all targets with optimizing flags"
	@echo "prf		-- build all targets with profiling flags"
	@echo "clean		-- delete temporary files"
	@echo "mostlyclean	-- also delete all generated files"
	@echo "distclean	-- delete also database and server log files"
	@echo ""
	@echo "benchmark execution targets:"
	@echo "run.crund (generic driver, uses property 'loads')"
	@echo "run.ndbapi"

dep dbg opt prf clean mostlyclean clobber:
	ant $@
	@cd $(SRC) && $(MAKE) $(MFLAGS) $@

distclean:
	rm -rf scripts/ndblog
	rm -f *~
	ant $@
	rm -f ./.ant-targets-build.xml
	@cd $(SRC) && $(MAKE) $(MFLAGS) $@

#------------------------------------------------------------------------------

# read the generic settings
include	env.properties

run.ndbapi run.crund:	$(SRC)/crund
	$(SRC)/crund \
	-p crundRun.properties -p crundNdb.properties

#------------------------------------------------------------------------------
