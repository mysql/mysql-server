# This test can't be run with running server (--extern) as this uses
# load_file() on a file in the tree.
#
--source include/force_myisam_default.inc
--source include/have_myisam.inc
#
# Basic cleanup
#
#
--disable_warnings
drop table if exists t1,t2,t3,t4,t5,t6,t7;
--enable_warnings
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';

	
-- error 1071
create table t1 (a text, unique (a(2100))) engine=myisam; # should give an error
create table t1 (a text, key (a(2100))) engine=myisam;    # key is auto-truncated
show create table t1;
drop table t1;

