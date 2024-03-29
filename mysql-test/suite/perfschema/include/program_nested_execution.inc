#
# Execute the nested stored programs created in
# suite/include/perfschema/program_nested_setup.inc 
#

--echo #####################
--echo # Executing queries #
--echo #####################

CALL c1(42);
SELECT * FROM t1;
DELETE FROM t1;

CALL iotest("io1", "io2", 1);
SELECT * FROM t1 ORDER BY data DESC;
DELETE FROM t1;

SELECT fun(6,10);

INSERT INTO t1 VALUES (20,13);

SELECT * FROM t2;
