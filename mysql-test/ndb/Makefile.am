
benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test/ndb

test_SCRIPTS = 	\
install_ndbcluster \
stop_ndbcluster

EXTRA_SCRIPTS = \
install_ndbcluster.sh \
stop_ndbcluster.sh

test_DATA = ndb_config_2_node.ini

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''ndbbindir''@!$(ndbbindir)!g' \
	  -e 's!@''ndbtoolsdir''@!$(ndbtoolsdir)!g' \
	  $< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

# Don't update the files from bitkeeper
%::SCCS/s.%
