# Copyright 2000-2008 MySQL AB, 2009 Sun Microsystems, Inc.
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

# Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS =	foreign

# These are built from source in the Docs directory
EXTRA_DIST =		INSTALL-SOURCE INSTALL-WIN-SOURCE \
			README COPYING EXCEPTIONS-CLIENT CMakeLists.txt

SUBDIRS =		. include @docs_dirs@ @zlib_dir@ \
			@readline_topdir@ sql-common scripts \
			@pstack_dir@ libservices \
			@sql_union_dirs@ unittest \
			@sql_server@ @man_dirs@ tests \
			netware @libmysqld_dirs@ \
			mysql-test support-files sql-bench \
			win

DIST_SUBDIRS =		. include Docs zlib \
			cmd-line-utils sql-common scripts \
			pstack libservices \
			strings mysys dbug extra regex libmysql libmysql_r client unittest storage plugin \
			vio sql man tests \
			netware libmysqld \
			mysql-test support-files sql-bench \
			win \
			BUILD
DISTCLEANFILES = ac_available_languages_fragment

# Create permission databases
init-db:		all
	$(top_builddir)/scripts/mysql_install_db

bin-dist:		all
	$(top_builddir)/scripts/make_binary_distribution @MAKE_BINARY_DISTRIBUTION_OPTIONS@

# Remove BK's "SCCS" subdirectories from source distribution
# Create initial database files for Windows installations and check them.
dist-hook:
	rm -rf `find $(distdir) -type d -name SCCS -print`
	mkdir -p $(distdir)/win
	scripts/mysql_install_db --no-defaults --cross-bootstrap \
		--builddir=$(top_builddir) \
		--datadir=$(distdir)/win/data \
		--srcdir=$(top_srcdir)
	storage/myisam/myisamchk --silent --fast $(distdir)/win/data/mysql/*.MYI

all-local:		@ABI_CHECK@	

tags:
	support-files/build-tags

.PHONY:	init-db bin-dist \
  test    test-force    test-full    test-force-full    test-force-mem \
  test-pl test-force-pl test-full-pl test-force-full-pl test-force-pl-mem \
  test-unit test-ps test-nr test-pr test-ns test-binlog-statement \
  test-ext-funcs test-ext-rpl test-ext-partitions test-ext-jp \
  test-ext-stress test-ext test-embedded test-reprepare \
  test-fast test-fast-cursor test-fast-view test-fast-prepare \
  test-full-qa

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "mysql-test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-unit:
	cd unittest && $(MAKE) test

test-ps:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) --ps-protocol --mysqld=--binlog-format=mixed

test-nr:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) --mysqld=--binlog-format=row

test-pr:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) $(mem) --ps-protocol --mysqld=--binlog-format=row

test-ns:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) $(mem) --mysqld=--binlog-format=mixed ; \
	    @PERL@ ./mysql-test-run.pl $(force) $(mem) --suite=funcs_1

test-binlog-statement:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) --mysqld=--binlog-format=statement

# This code is duplicated in "test-bt", see the Changeset comment of 2007-Dec-07
test-embedded:
	if [ -e bin/mysqltest_embedded -o -e libmysqld/examples/mysqltest_embedded ] ; then \
	  cd mysql-test ; MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=embedded --force --timer \
	          --embedded-server --skip-rpl --skip-ndbcluster ; \
	else \
	  echo "no program found for 'embedded' tests - skipped testing" ; \
	fi

test-reprepare:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(force) $(mem) --ps-protocol \
		--mysqld=--debug=+d,reprepare_each_statement

test:	test-unit test-ns test-pr

smoke:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --do-test=s

test-full:	test test-nr test-ps

test-force:
	$(MAKE) force=--force test

test-force-full:
	$(MAKE) force=--force test-full

#used by autopush.pl to run memory based tests
test-force-mem:
	$(MAKE) force=--force mem=--mem test

EXP = --experimental=collections/default.experimental

test-bt:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --comment=normal --force --timer \
	        --skip-ndbcluster --report-features $(EXP)
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl  --comment=ps    --force --timer \
	        --skip-ndbcluster --ps-protocol $(EXP)
	-if [ -e bin/ndbd -o -e storage/ndb/src/kernel/ndbd ] ; then \
	  cd mysql-test ; \
	    MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=ndb+rpl_ndb+ps --force --timer \
	          --ps-protocol --mysqld=--binlog-format=row --suite=ndb,rpl_ndb $(EXP) ; \
	    MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=ndb --force --timer \
	          --with-ndbcluster-only $(EXP) ; \
	else \
	  echo "no program found for 'ndbcluster' tests - skipped testing" ; \
	fi
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=funcs1+ps --ps-protocol --reorder --suite=funcs_1 $(EXP)
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=funcs2 --suite=funcs_2 $(EXP)
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=partitions --suite=parts $(EXP)
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=stress --suite=stress $(EXP)
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=jp --suite=jp $(EXP)
	-if [ -d mysql-test/suite/nist ] ; then \
	  cd mysql-test ; MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=nist --force --suite=nist $(EXP) ; \
	fi
	-if [ -d mysql-test/suite/nist ] ; then \
	  cd mysql-test ; MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=nist+ps --force --suite=nist --ps-protocol $(EXP) ; \
	fi
	-if [ -e bin/mysqltest_embedded -o -e libmysqld/examples/mysqltest_embedded ] ; then \
	  cd mysql-test ; MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=embedded --force --timer \
	          --embedded-server --skip-rpl --skip-ndbcluster $(EXP) ; \
	else \
	  echo "no program found for 'embedded' tests - skipped testing" ; \
	fi

test-bt-fast:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl  --comment=ps    --force --timer \
	        --skip-ndbcluster --ps-protocol --report-features $(EXP)
	-if [ -e bin/ndbd -o -e storage/ndb/src/kernel/ndbd ] ; then \
	  cd mysql-test ; \
	    MTR_BUILD_THREAD=auto \
	      @PERL@ ./mysql-test-run.pl --comment=ndb --force --timer \
	          --with-ndbcluster-only $(EXP) ; \
	else \
	  echo "no program found for 'ndbcluster' tests - skipped testing" ; \
	fi
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=stress --suite=stress $(EXP)

test-bt-fast:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --force --comment=ps --ps-protocol --report-features

test-bt-debug:
	-cd mysql-test ; MTR_BUILD_THREAD=auto \
	    @PERL@ ./mysql-test-run.pl --comment=debug  --force --timer \
	        --skip-ndbcluster --skip-rpl --report-features $(EXP)

test-bt-debug-fast:

test-bt-debug-fast:

# Keep these for a while
test-pl:	test
test-full-pl:	test-full
test-force-pl:	test-force
test-force-pl-mem:  test-force-mem
test-force-full-pl: test-force-full

test-ext-funcs:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --force --reorder --suite=funcs_1 ; \
	    @PERL@ ./mysql-test-run.pl --force --suite=funcs_2

test-ext-rpl:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --force --suite=rpl

test-ext-partitions:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --force --suite=parts

test-ext-jp:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --force --suite=jp

test-ext-stress:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl --force --big-test --suite=stress

test-ext:	test-ext-funcs test-ext-rpl test-ext-partitions test-ext-jp test-ext-stress

test-fast:
	cd mysql-test ; \
	    @PERL@ ./mysql-test-run.pl $(subset) --force --skip-ndb --skip-innodb --skip-im --skip-rpl ; \
	    @PERL@ ./mysql-test-run.pl $(subset) --force --suite=funcs_1 --do-test=myisam ; \
	    @PERL@ ./mysql-test-run.pl $(subset) --force --suite=stress --do-test=ddl_myisam 

test-fast-view:
	$(MAKE) subset=--view-protocol test-fast

test-fast-cursor:
	$(MAKE) subset=--cursor-protocol test-fast

test-fast-prepare:
	$(MAKE) subset=--ps-protocol test-fast

test-full-qa:
	$(MAKE) force=--force test-pr \
	    test-binlog-statement test-ext test-fast-view \
	        test-fast-cursor test-unit

#
# Headers which need to be checked for abi/api compatibility.
# API_PREPROCESSOR_HEADER will be used until mysql_priv.h stablizes
# after which TEST_PREPROCESSOR_HEADER will be used.
#

API_PREPROCESSOR_HEADER = $(top_srcdir)/include/mysql/plugin.h \
                           $(top_srcdir)/include/mysql.h

TEST_PREPROCESSOR_HEADER = $(top_srcdir)/include/mysql/plugin.h \
                            $(top_srcdir)/sql/mysql_priv.h \
                            $(top_srcdir)/include/mysql.h

#
# Rules for checking that the abi/api has not changed.
#
# The following steps are followed in the do_abi_check rule below
#
# 1) Generate preprocessor output for the files that need to
#    be tested for abi/api changes. use -nostdinc to prevent
#    generation of preprocessor output for system headers. This
#    results in messages in stderr saying that these headers
#    were not found. Redirect the stderr output to /dev/null
#    to prevent seeing these messages.
# 2) sed the output to 
#    2.1) remove blank lines and lines that begin with "# "
#    2.2) When gcc -E is run on the Mac OS  and solaris sparc platforms it
#         introduces a line of output that shows up as a difference between
#         the .pp and .out files. Remove these OS specific preprocessor text
#         inserted by the preprocessor.
# 3) diff the generated file and the canons (.pp files already in
#    the repository).
# 4) delete the .out file that is generated.
#
# If the diff fails, the generated file is not removed. This will
# be useful for analysis of ABI differences (e.g. using a visual
# diff tool).
#
# A ABI change that causes a build to fail will always be accompanied
# by new canons (.out files). The .out files that are not removed will
# be replaced as the new .pp files.
#
# e.g. If include/mysql/plugin.h has an ABI change then this rule would
# leave a <build directory>/abi_check.out file.
#
# A developer with a justified API change will then do a
# mv <build directory>/abi_check.out include/mysql/plugin.pp 
# to replace the old canons with the new ones.
#

abi_check:	$(API_PREPROCESSOR_HEADER)
	$(MAKE) abi_headers="$^" do_abi_check

abi_check_all:	$(TEST_PREPROCESSOR_HEADER)
	$(MAKE) abi_headers="$^" do_abi_check

do_abi_check:
	set -ex; \
	for file in $(abi_headers); do \
	         @CC@ -E -nostdinc -dI \
	                  -I$(top_srcdir)/include \
	                  -I$(top_srcdir)/include/mysql \
	                  -I$(top_srcdir)/sql \
	                  -I$(top_builddir)/include \
	                  -I$(top_builddir)/include/mysql \
	                  -I$(top_builddir)/sql \
	                                 $$file 2>/dev/null | \
	                  @SED@ -e '/^# /d' \
	                            -e '/^[ 	]*$$/d' \
	                            -e '/^#pragma GCC set_debug_pwd/d' \
	                            -e '/^#ident/d' > \
	                                       $(top_builddir)/abi_check.out; \
	                  @DIFF@ -w $$file.pp $(top_builddir)/abi_check.out; \
	                  @RM@ $(top_builddir)/abi_check.out; \
	done

# Don't update the files from bitkeeper
%::SCCS/s.%
