--echo #
--echo # Bug #29952775 SETTING SORT_BUFFER_SIZE TO A LARGE VALUE
--echo #               CAUSES QUERY TO GO OUT OF MEMORY
--echo #

# The provided value is too large for a Sys_var_ulong on 32bit systems
# The provided value is too large for a Sys_var_ulong on Windows
--source include/not_windows.inc

SET SESSION sort_buffer_size = 18446744073709551615;
CREATE TABLE t0(c0 INT UNIQUE, c1 INT UNIQUE);
INSERT INTO t0(c0) VALUES(1), (2), (3);
SELECT * FROM t0 WHERE NOT((t0.c1 IS NULL) AND ((t0.c0) != (1)));
DROP TABLE t0;
SET SESSION sort_buffer_size = default;
