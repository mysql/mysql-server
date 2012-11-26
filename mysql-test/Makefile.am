# Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

## Process this file with automake to create Makefile.in

testroot =	$(prefix)
testdir =	$(testroot)/mysql-test

test_SCRIPTS =	mtr \
		mysql-test-run \
		mysql-test-run.pl \
		mysql-stress-test.pl

nobase_test_DATA = \
	valgrind.supp \
	lib/v1/mysql-test-run.pl \
	lib/v1/mtr_cases.pl \
	lib/v1/mtr_io.pl \
	lib/v1/mtr_report.pl \
	lib/v1/My/Config.pm \
	lib/v1/mtr_gcov.pl \
	lib/v1/mtr_match.pl \
	lib/v1/mtr_stress.pl \
	lib/v1/ndb_config_1_node.ini \
	lib/v1/ndb_config_2_node.ini \
	lib/v1/mtr_gprof.pl \
	lib/v1/mtr_misc.pl \
	lib/v1/mtr_timer.pl \
	lib/v1/mtr_im.pl \
	lib/v1/mtr_process.pl \
	lib/v1/mtr_unique.pl \
	lib/mtr_cases.pm \
	lib/mtr_gcov.pl \
	lib/mtr_gprof.pl \
	lib/mtr_io.pl \
	lib/mtr_match.pm \
	lib/mtr_misc.pl \
	lib/mtr_process.pl \
	lib/mtr_report.pm \
	lib/mtr_stress.pl \
	lib/mtr_unique.pm \
	lib/My/ConfigFactory.pm \
	lib/My/Config.pm \
	lib/My/Find.pm \
	lib/My/Handles.pm \
	lib/My/Options.pm \
	lib/My/Test.pm \
	lib/My/Platform.pm \
	lib/My/SafeProcess.pm \
	lib/My/File/Path.pm \
	lib/My/SysInfo.pm \
	lib/My/CoreDump.pm \
	lib/My/SafeProcess/Base.pm

SUBDIRS =	lib/My/SafeProcess

EXTRA_DIST =	README \
		$(test_SCRIPTS) \
		$(nobase_test_DATA)

# List of directories containing test + result files and the
# related test data files that should be copied
TEST_DIRS = t r include std_data std_data/parts collections \
	std_data/ndb_backup50 std_data/ndb_backup51 \
	std_data/ndb_backup51_data_be std_data/ndb_backup51_data_le \
	std_data/funcs_1 \
	extra/binlog_tests/ extra/rpl_tests \
	suite/binlog suite/binlog/t suite/binlog/r suite/binlog/std_data \
	suite/federated \
	suite/federated/include \
	suite/funcs_1 suite/funcs_1/bitdata \
	suite/funcs_1/include suite/funcs_1/lib suite/funcs_1/r \
	suite/funcs_1/t suite/funcs_1/views suite/funcs_1/cursors \
	suite/funcs_1/datadict suite/funcs_1/storedproc suite/funcs_1/triggers \
	suite/funcs_2 suite/funcs_2/charset suite/funcs_2/data \
	suite/funcs_2/include suite/funcs_2/lib suite/funcs_2/r \
	suite/funcs_2/t \
	suite/jp suite/jp/t suite/jp/r suite/jp/std_data suite/jp/include \
	suite/manual/t suite/manual/r \
	suite/ndb_team suite/ndb_team/t suite/ndb_team/r \
	suite/rpl suite/rpl/include suite/rpl/r \
	suite/rpl/t \
	suite/stress/include suite/stress/t suite/stress/r \
	suite/ndb suite/ndb/t suite/ndb/r \
	suite/rpl_ndb suite/rpl_ndb/t suite/rpl_ndb/r \
	suite/parts suite/parts/t suite/parts/r suite/parts/inc \
	suite/innodb suite/innodb/t suite/innodb/r suite/innodb/include \
	suite/innodb_plugin suite/innodb_plugin/t suite/innodb_plugin/r \
	suite/innodb_plugin/include \
	suite/engines suite/engines/funcs suite/engines/iuds suite/engines/rr_trx \
	suite/engines/funcs/r suite/engines/funcs/t suite/engines/iuds/r \
	suite/engines/iuds/t suite/engines/rr_trx/include suite/engines/rr_trx/r \
	suite/engines/rr_trx/t

# Used by dist-hook and install-data-local to copy all
# test files into either dist or install directory
install_test_files:
	@if test -z "$(INSTALL_TO_DIR)"; then \
	  echo "Set INSTALL_TO_DIR!" && exit 1; \
	fi
	@for dir in $(TEST_DIRS); do \
	  from_dir="$(srcdir)/$$dir"; \
	  to_dir="$(INSTALL_TO_DIR)/$$dir"; \
	  $(mkinstalldirs) "$$to_dir"; \
	  for f in `(cd $$from_dir && ls)`; do \
	    if test -f "$$from_dir/$$f"; then \
	      $(INSTALL_DATA) "$$from_dir/$$f" "$$to_dir/$$f" ; \
	    fi; \
	  done \
	done

dist-hook:
	$(MAKE)	INSTALL_TO_DIR="$(distdir)" install_test_files

install-data-local:
	$(MAKE)	INSTALL_TO_DIR="$(DESTDIR)$(testdir)" install_test_files

uninstall-local:
	@RM@ -f -r $(DESTDIR)$(testdir)

# mtr - a shortcut for executing mysql-test-run.pl
mtr:
	$(RM) -f mtr
	$(LN_S) mysql-test-run.pl mtr

# mysql-test-run - a shortcut for executing mysql-test-run.pl
mysql-test-run:
	$(RM) -f mysql-test-run
	$(LN_S) mysql-test-run.pl mysql-test-run

# Don't update the files from bitkeeper
%::SCCS/s.%
