# include/have_hypergraph.inc
#
# Enable use of the hypergraph optimizer in the current test if
# it is available. Skip the test if mysqld was built without support
# for the hypergraph optimizer.
#
# Tests that enable the hypergraph optimizer should disable it when
# they are done using this command:
#
#     --source include/disable_hypergraph.inc

--disable_query_log
--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET @@session.optimizer_switch = 'hypergraph_optimizer=on',
    @@global.optimizer_switch = 'hypergraph_optimizer=on';
if (`SELECT NOT FIND_IN_SET('hypergraph_optimizer=on', @@optimizer_switch)`) {
   --skip Test requires binaries built with WITH_HYPERGRAPH_OPTIMIZER=ON
}
--enable_query_log
