# The include statement below is a temp one for tests that are yet to
#be ported to run with InnoDB,
#but needs to be kept for tests that would need MyISAM in future.
--source include/force_myisam_default.inc
--source include/have_myisam.inc

# 
# Run null_key.inc with all of the so-called 6.0 features.
#

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on';

--source include/null_key_myisam.inc

set optimizer_switch=default;

