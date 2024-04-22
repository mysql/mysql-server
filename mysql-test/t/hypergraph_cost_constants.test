--source include/have_hypergraph.inc
--disable_warnings

# This test verifies that changes to the value of optimizer cost constants by
# modifying the tables mysql.engine_cost and mysql.server_cost only affects
# the original optimizer, and not the hypergraph optimizer.

# The test looks at the cost in the EXPLAIN output before and after modifying
# (and re-loading) cost constants. We want to ensure that both optimizers pick
# plans that utilize the modified cost constants, and that only the cost
# reported by the original optimizer changes.

# First we look at the effect of doubling the engine cost constants on a simple
# table scan. Both the original and hypergraph optimizer use the engine cost
# constants when computing the cost of a table scan.

# The second test doubles the server cost constants and looks at the cost of
# a covering index range scan. With hypergraph + InnoDB the default handler
# implementation of multi_range_read_info_const() is called which uses the
# server cost constant row_evaluate_cost, among other calls to the cost model.

-- echo #
-- echo # First test: engine costs.
-- echo #

CREATE TABLE t(x INT);
INSERT INTO t VALUES (0), (1), (2);
ANALYZE TABLE t;

-- echo #
-- echo # Default engine costs.
-- echo #

SET optimizer_switch='hypergraph_optimizer=off';
EXPLAIN FORMAT=TREE SELECT * FROM t;

SET optimizer_switch='hypergraph_optimizer=on';
EXPLAIN FORMAT=TREE SELECT * FROM t;

-- echo #
-- echo # Double engine costs.
-- echo #

UPDATE mysql.engine_cost SET cost_value = 2*default_value;
--source include/flush_costs_and_reconnect.inc

SET optimizer_switch='hypergraph_optimizer=off';
EXPLAIN FORMAT=TREE SELECT * FROM t;

SET optimizer_switch='hypergraph_optimizer=on';
EXPLAIN FORMAT=TREE SELECT * FROM t;

UPDATE mysql.engine_cost SET cost_value = DEFAULT;
--source include/flush_costs_and_reconnect.inc

-- echo #
-- echo # Second test: server costs.
-- echo #

DROP TABLE t;
CREATE TABLE t(x INT PRIMARY KEY);
INSERT INTO t VALUES (0), (1), (2), (3), (4), (5), (6), (7), (8), (9);
ANALYZE TABLE t;

-- echo #
-- echo # Default server costs.
-- echo #

SET optimizer_switch='hypergraph_optimizer=off';
EXPLAIN FORMAT=TREE SELECT * FROM t WHERE x < 5;

SET optimizer_switch='hypergraph_optimizer=on';
EXPLAIN FORMAT=TREE SELECT * FROM t WHERE x < 5;

-- echo #
-- echo # Double server costs.
-- echo #

UPDATE mysql.server_cost SET cost_value = 2*default_value;
--source include/flush_costs_and_reconnect.inc

SET optimizer_switch='hypergraph_optimizer=off';
EXPLAIN FORMAT=TREE SELECT * FROM t WHERE x < 5;

SET optimizer_switch='hypergraph_optimizer=on';
EXPLAIN FORMAT=TREE SELECT * FROM t WHERE x < 5;

UPDATE mysql.server_cost SET cost_value = DEFAULT;
--source include/flush_costs_and_reconnect.inc

DROP TABLE t;
--enable_warnings

--source include/disable_hypergraph.inc
