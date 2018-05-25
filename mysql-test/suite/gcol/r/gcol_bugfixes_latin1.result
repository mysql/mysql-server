SET NAMES latin1;
#
# Bug#22991924 GCOLS: UTF16 STRING OPERATIONS GIVE DOUBLED \0\0 STRINGS
# ON EACH TABLE REBUILD
#
CREATE TABLE t1 ( gc VARCHAR( 20 ) charset latin1 GENERATED ALWAYS AS ( 'Å' ) );
INSERT INTO t1 VALUES ();
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `gc` varchar(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci GENERATED ALWAYS AS (_latin1'?') VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT HEX( gc ) FROM t1;
HEX( gc )
C5
OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
SELECT HEX( gc ) FROM t1;
HEX( gc )
C5
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `gc` varchar(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci GENERATED ALWAYS AS (_latin1'?') VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET NAMES utf8;
Warnings:
Warning	3719	'utf8' is currently an alias for the character set UTF8MB3, but will be an alias for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be unambiguous.
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `gc` varchar(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci GENERATED ALWAYS AS (_latin1'Å') VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SELECT HEX( gc ) FROM t1;
HEX( gc )
C5
OPTIMIZE TABLE t1;
Table	Op	Msg_type	Msg_text
test.t1	optimize	note	Table does not support optimize, doing recreate + analyze instead
test.t1	optimize	status	OK
SELECT HEX( gc ) FROM t1;
HEX( gc )
C5
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `gc` varchar(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci GENERATED ALWAYS AS (_latin1'Å') VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET NAMES latin1;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `gc` varchar(20) CHARACTER SET latin1 COLLATE latin1_swedish_ci GENERATED ALWAYS AS (_latin1'?') VIRTUAL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
SET NAMES DEFAULT;
DROP TABLE t1;
