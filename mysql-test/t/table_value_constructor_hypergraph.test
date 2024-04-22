--source include/have_hypergraph.inc
--source include/table_value_constructor.inc

--echo #
--echo # Bug#31949057: TABLE VALUE CONSTRUCTOR FOLLOWED BY ORDER BY
--echo #               IS NOT SORTED CORRECTLY
--echo #

VALUES ROW(1,9), ROW(2,4) ORDER BY column_1;

(VALUES ROW(1,2), ROW(3,4), ROW(-1,2) ORDER BY column_0) ORDER BY column_0;

VALUES ROW(1, 1), ROW(10, 2), ROW(2, 3), ROW(3, 4), ROW(5, 5)
ORDER BY 1 LIMIT 2 OFFSET 1;

VALUES ROW(0, 1), ROW(4, 6), ROW(7, 5), ROW(5, 7), ROW(9, 8),
       ROW(7, 2), ROW(7, 6), ROW(4, 5), ROW(3, 2), ROW(9, 7)
ORDER BY 1, 2;

--source include/disable_hypergraph.inc
