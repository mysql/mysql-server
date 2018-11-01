# Restart server with --tmpdir
# restart: --tmpdir=MYSQLTEST_VARDIR/tmp --temptable_max_ram=1M --log_error_verbosity=3 --log-error=MYSQLTEST_VARDIR/log/tmpdir_basic.err
# Test with --tmpdir
SET DEBUG='+d,enter';
SET SESSION cte_max_recursion_depth = 20000;
WITH RECURSIVE cte (n, val1, val2, val3, val4, val5) AS
(SELECT 1, UUID(), UUID(), UUID(), UUID(), UUID()
UNION ALL
SELECT n + 1 + IF(n > 18000, SLEEP(0.01), 0),
UUID(), UUID(), UUID(), UUID(), UUID()
FROM cte WHERE n < 20000
)
SELECT COUNT(*) FROM cte;
COUNT(*)
20000
SET DEBUG='-d,enter';
# restart: --temptable_max_ram=1M --log_error_verbosity=3 --log-error=MYSQLTEST_VARDIR/log/tmpdir_basic_default.err
# Test with default tmpdir
SET DEBUG='+d,enter';
SET SESSION cte_max_recursion_depth = 20000;
WITH RECURSIVE cte (n, val1, val2, val3, val4, val5) AS
(SELECT 1, UUID(), UUID(), UUID(), UUID(), UUID()
UNION ALL
SELECT n + 1 + IF(n > 18000, SLEEP(0.01), 0),
UUID(), UUID(), UUID(), UUID(), UUID()
FROM cte WHERE n < 20000
)
SELECT COUNT(*) FROM cte;
COUNT(*)
20000
SET DEBUG='-d,enter';
# restart
