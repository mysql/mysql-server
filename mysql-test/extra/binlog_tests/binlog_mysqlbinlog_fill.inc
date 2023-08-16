# ==== Purpose ====
#
# Add [position|datetime] data to mysqlbinlog.  To generate a binary log with
# a few deterministic,increasing timestamps.
#
# ==== Implementation ====

# We need to set fixed timestamps in this test.
# Use a date in the future to keep a growing timestamp along the
# binlog (including the Start_log_event). This test will work
# unchanged everywhere, because mysql-test-run has fixed TZ, which it
# exports (so mysqlbinlog has same fixed TZ).
#
# ===== Assumption ========
# As this is a generic file and be used by other test, So we have made an
# assumption that a table is already created with defination like
# CREATE TABLE t1 (a INT);
#

# Binlog 1:
# One transaction with timestamp 2031-01-01 12:00:00
--let $datetime_1= 2031-01-01 12:00:00
eval SET TIMESTAMP= UNIX_TIMESTAMP("$datetime_1");
INSERT INTO t1 VALUES(1);
--let $pos_1= query_get_value(SHOW BINARY LOG STATUS, Position, 1)

# One transaction with timestamp 2032-01-01 12:00:00
--let $datetime_2= 2032-01-01 12:00:00
eval SET TIMESTAMP= UNIX_TIMESTAMP("$datetime_2");
INSERT INTO t1 VALUES(2);
--let $pos_2= query_get_value(SHOW BINARY LOG STATUS, Position, 1)

# One transaction with timestamp 2033-01-01 12:00:00
eval SET TIMESTAMP= UNIX_TIMESTAMP("2033-01-01 12:00:00");
INSERT INTO t1 VALUES(3);

--let $file_1= query_get_value(SHOW BINARY LOG STATUS, File, 1)
FLUSH LOGS;

#Binlog 2:
# One transaction with timestamp 2034-01-01 12:00:00
SET TIMESTAMP= UNIX_TIMESTAMP("2034-01-01 12:00:00");
INSERT INTO t1 VALUES(4);
--let $pos_3= query_get_value(SHOW BINARY LOG STATUS, Position, 1)

# One transaction with timestamp 2035-01-01 12:00:00
--let $datetime_3= 2035-01-01 12:00:00
eval SET TIMESTAMP= UNIX_TIMESTAMP("$datetime_3");
INSERT INTO t1 VALUES(5);

--let $file_2= query_get_value(SHOW BINARY LOG STATUS, File, 1)
FLUSH LOGS;

