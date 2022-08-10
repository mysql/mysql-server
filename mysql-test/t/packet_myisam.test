# Windows fails because it disconnects on too-large packets instead of just
# swallowing them and returning an error
--source include/not_windows.inc

# Save the initial number of concurrent sessions
--source include/count_sessions.inc
# Test need MyISAM storage engine
--source include/force_myisam_default.inc
--source include/have_myisam.inc


#
# Check protocol handling
#

set @max_allowed_packet=@@global.max_allowed_packet;
set @net_buffer_length=@@global.net_buffer_length;


--echo #
--echo # Bug #20376498: MAX_ALLOWED_PACKET ERROR DESTROYS
--echo #                ORIGINAL DATA


CREATE TABLE t3 (c31 INT NOT NULL, c32 LONGTEXT,
                 PRIMARY KEY (c31)) charset latin1 ENGINE=MYISAM;
CREATE TABLE t4 (c41 INT NOT NULL, c42 LONGTEXT,
                 PRIMARY KEY (c41)) charset latin1 ENGINE=MYISAM;

INSERT INTO t3 VALUES(100,'a');
INSERT INTO t3 VALUES(111,'abcd');
INSERT INTO t3 VALUES(122,'b');

INSERT INTO t4
SELECT c31, CONCAT(c32,
                   REPEAT('a', @max_allowed_packet-1))
FROM t3;

SELECT c41, LENGTH(c42) FROM t4;

--skip_if_hypergraph  # Multi-table code path gives warning instead of error.
--error ER_WARN_ALLOWED_PACKET_OVERFLOWED
UPDATE t3
SET c32= CONCAT(c32,
                REPEAT('a', @max_allowed_packet-1));

--skip_if_hypergraph  # Depends on the skipped UPDATE.
SELECT c31, LENGTH(c32) FROM t3;

DROP TABLE t3, t4;

