--source include/no_valgrind_without_big.inc
# Skipping the test when binlog_format=STATEMENT due to unsafe statements:
# unsafe system function like rand(), LIMIT clause.
--source include/rpl/deprecated/not_binlog_format_statement.inc

# 
# Run subquery_sj.inc with all of the so-called 6.0 features.
#

set optimizer_switch='semijoin=on,materialization=on,firstmatch=on,loosescan=on,index_condition_pushdown=on,mrr=on,mrr_cost_based=off';

--source include/subquery_sj.inc

set optimizer_switch=default;
