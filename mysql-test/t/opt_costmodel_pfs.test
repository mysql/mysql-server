#
# Cost model tests that requires performance schema
#

--echo #
--echo # Bug#20755430 COST CONSTANT CACHE MUTEX NOT INSTRUMENTED BY
--echo #              PERFORMANCE SCHEMA
--echo #

# Verify that the Cost constant cache mutex is instrumented
SELECT * FROM performance_schema.setup_instruments
WHERE NAME LIKE 'Wait/Synch/Mutex/sql/Cost_constant_cache%';
