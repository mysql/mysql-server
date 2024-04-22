# Disable the hypergraph optimizer in tests that have enabled it by
# including include/have_hypergraph.inc.

--disable_query_log
SET @@session.optimizer_switch = 'hypergraph_optimizer=off',
    @@global.optimizer_switch = 'hypergraph_optimizer=off';
--enable_query_log
