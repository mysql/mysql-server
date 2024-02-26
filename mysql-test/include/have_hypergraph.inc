# include/have_hypergraph.inc
#
# Enable use of the hypergraph optimizer in the current connection if
# it is available. Skip the test if mysqld was built without support
# for the hypergraph optimizer.

--disable_query_log
--disable_warnings

--error 0, ER_HYPERGRAPH_NOT_SUPPORTED_YET
SET optimizer_switch = 'hypergraph_optimizer=on';
if (`SELECT NOT FIND_IN_SET('hypergraph_optimizer=on', @@optimizer_switch)`) {
   --skip Test requires binaries built with WITH_HYPERGRAPH_OPTIMIZER=ON
}
--enable_warnings
--enable_query_log
