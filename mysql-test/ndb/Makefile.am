
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
	  -e 's!@''ndb_port_base''@!$(ndb_port_base)!g' \
	  -e 's!@''ndbbindir''@!$(ndbbindir)!g' \
	  -e 's!@''ndbtoolsdir''@!$(ndbtoolsdir)!g' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
