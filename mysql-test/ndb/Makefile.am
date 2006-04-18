
benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test/ndb

EXTRA_DIST = ndbcluster.sh
CLEANFILES = ndbcluster
dist_test_DATA = ndb_config_2_node.ini ndb_config_1_node.ini
test_SCRIPTS = ndbcluster

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
