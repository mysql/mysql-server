# The proper term is pseudo_replica_mode, but we use this compatibility alias
# to make the statement usable on server versions 8.0.24 and older.
/*!50530 SET @@SESSION.PSEUDO_SLAVE_MODE=1*/;
/*!50003 SET @OLD_COMPLETION_TYPE=@@COMPLETION_TYPE,COMPLETION_TYPE=0*/;
DELIMITER /*!*/;
# at 4
#241223  9:07:13 server id 1  end_log_pos 127 CRC32 0x3d47d319 	Start: binlog v 4, server v 9.0.0-debug created 241223  9:07:13 at startup
ROLLBACK/*!*/;
BINLOG '
Ef5oZw8BAAAAewAAAH8AAAAAAAQAOS4wLjAtZGVidWcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAAAAAR/mhnEwANAAgAAAAABAAEAAAAYwAEGggAAAAAAAACAAAACgoKKioAEjQA
CigAAAEZ00c9
'/*!*/;
# at 127
#241223  9:07:13 server id 1  end_log_pos 158 CRC32 0x4e5c0577 	Previous-GTIDs
# [empty]
# at 158
#241223  9:07:13 server id 1  end_log_pos 235 CRC32 0x866da6b2 	Anonymous_GTID	last_committed=0	sequence_number=1	rbr_only=no	original_committed_timestamp=1734934033884642	immediate_commit_timestamp=1734934033884642	transaction_length=201
# original_commit_timestamp=1734934033884642 (2024-12-23 09:07:13.884642 GMT)
# immediate_commit_timestamp=1734934033884642 (2024-12-23 09:07:13.884642 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934033884642*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 235
#241223  9:07:13 server id 1  end_log_pos 359 CRC32 0x5d96f42a 	Query	thread_id=9	exec_time=0	error_code=0	Xid = 58
use `test`/*!*/;
SET TIMESTAMP=1734934033/*!*/;
SET debug="+d,fk";
SET @@session.pseudo_thread_id=9/*!*/;
SET @@session.foreign_key_checks=1, @@session.sql_auto_is_null=0, @@session.unique_checks=1, @@session.autocommit=1/*!*/;
SET @@session.sql_mode=1168113696/*!*/;
SET @@session.auto_increment_increment=1, @@session.auto_increment_offset=1/*!*/;
/*!\C utf8mb4 *//*!*/;
SET @@session.character_set_client=255,@@session.collation_connection=255,@@session.collation_server=255/*!*/;
SET @@session.lc_time_names=0/*!*/;
SET @@session.collation_database=DEFAULT/*!*/;
/*!80011 SET @@session.default_collation_for_utf8mb4=255*//*!*/;
/*!80013 SET @@session.sql_require_primary_key=0*//*!*/;
CREATE TABLE t1 (f1 INT PRIMARY KEY)
/*!*/;
# at 359
#241223  9:07:13 server id 1  end_log_pos 438 CRC32 0x45b22d7c 	Anonymous_GTID	last_committed=1	sequence_number=2	rbr_only=no	original_committed_timestamp=1734934033954170	immediate_commit_timestamp=1734934033954170	transaction_length=283
# original_commit_timestamp=1734934033954170 (2024-12-23 09:07:13.954170 GMT)
# immediate_commit_timestamp=1734934033954170 (2024-12-23 09:07:13.954170 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934033954170*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 438
#241223  9:07:13 server id 1  end_log_pos 642 CRC32 0xb06251b6 	Query	thread_id=9	exec_time=0	error_code=0	Xid = 59
SET TIMESTAMP=1734934033/*!*/;
/*!80013 SET @@session.sql_require_primary_key=0*//*!*/;
CREATE TABLE t2 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE CASCADE ON UPDATE CASCADE)
/*!*/;
# at 642
#241223  9:07:13 server id 1  end_log_pos 721 CRC32 0xeae3cc82 	Anonymous_GTID	last_committed=2	sequence_number=3	rbr_only=no	original_committed_timestamp=1734934034021305	immediate_commit_timestamp=1734934034021305	transaction_length=285
# original_commit_timestamp=1734934034021305 (2024-12-23 09:07:14.021305 GMT)
# immediate_commit_timestamp=1734934034021305 (2024-12-23 09:07:14.021305 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034021305*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 721
#241223  9:07:13 server id 1  end_log_pos 927 CRC32 0x7ed5e6cc 	Query	thread_id=9	exec_time=1	error_code=0	Xid = 60
SET TIMESTAMP=1734934033/*!*/;
/*!80013 SET @@session.sql_require_primary_key=0*//*!*/;
CREATE TABLE t3 (f1 INT PRIMARY KEY, f2 INT, FOREIGN KEY (f2) REFERENCES t1(f1) ON DELETE SET NULL ON UPDATE SET NULL)
/*!*/;
# at 927
#241223  9:07:14 server id 1  end_log_pos 1006 CRC32 0x49680de0 	Anonymous_GTID	last_committed=3	sequence_number=4	rbr_only=no	original_committed_timestamp=1734934034039041	immediate_commit_timestamp=1734934034039041	transaction_length=300
# original_commit_timestamp=1734934034039041 (2024-12-23 09:07:14.039041 GMT)
# immediate_commit_timestamp=1734934034039041 (2024-12-23 09:07:14.039041 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034039041*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 1006
#241223  9:07:14 server id 1  end_log_pos 1083 CRC32 0xf8961b5e 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 1083
#241223  9:07:14 server id 1  end_log_pos 1196 CRC32 0xba56052b 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
INSERT INTO t1 VALUES (1),(2),(3),(4),(5)
/*!*/;
# at 1196
#241223  9:07:14 server id 1  end_log_pos 1227 CRC32 0x13661b96 	Xid = 61
COMMIT/*!*/;
# at 1227
#241223  9:07:14 server id 1  end_log_pos 1306 CRC32 0xabe7ae9a 	Anonymous_GTID	last_committed=4	sequence_number=5	rbr_only=no	original_committed_timestamp=1734934034050968	immediate_commit_timestamp=1734934034050968	transaction_length=327
# original_commit_timestamp=1734934034050968 (2024-12-23 09:07:14.050968 GMT)
# immediate_commit_timestamp=1734934034050968 (2024-12-23 09:07:14.050968 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034050968*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 1306
#241223  9:07:14 server id 1  end_log_pos 1388 CRC32 0x77401bbe 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 1388
#241223  9:07:14 server id 1  end_log_pos 1523 CRC32 0x7f84cdf4 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
INSERT INTO t2 VALUES (1, 1),(2, 2),(3, 3), (4, 4), (5, 5)
/*!*/;
# at 1523
#241223  9:07:14 server id 1  end_log_pos 1554 CRC32 0xf79b7201 	Xid = 62
COMMIT/*!*/;
# at 1554
#241223  9:07:14 server id 1  end_log_pos 1633 CRC32 0xe2de2ab5 	Anonymous_GTID	last_committed=5	sequence_number=6	rbr_only=no	original_committed_timestamp=1734934034062826	immediate_commit_timestamp=1734934034062826	transaction_length=327
# original_commit_timestamp=1734934034062826 (2024-12-23 09:07:14.062826 GMT)
# immediate_commit_timestamp=1734934034062826 (2024-12-23 09:07:14.062826 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034062826*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 1633
#241223  9:07:14 server id 1  end_log_pos 1715 CRC32 0x49c445f4 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 1715
#241223  9:07:14 server id 1  end_log_pos 1850 CRC32 0x5cac18e4 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
INSERT INTO t3 VALUES (1, 1),(2, 2),(3, 3), (4, 4), (5, 5)
/*!*/;
# at 1850
#241223  9:07:14 server id 1  end_log_pos 1881 CRC32 0xe8266706 	Xid = 63
COMMIT/*!*/;
# at 1881
#241223  9:07:14 server id 1  end_log_pos 1960 CRC32 0x7c7a654f 	Anonymous_GTID	last_committed=6	sequence_number=7	rbr_only=no	original_committed_timestamp=1734934034072460	immediate_commit_timestamp=1734934034072460	transaction_length=284
# original_commit_timestamp=1734934034072460 (2024-12-23 09:07:14.072460 GMT)
# immediate_commit_timestamp=1734934034072460 (2024-12-23 09:07:14.072460 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034072460*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 1960
#241223  9:07:14 server id 1  end_log_pos 2037 CRC32 0xc58092a5 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 2037
#241223  9:07:14 server id 1  end_log_pos 2134 CRC32 0x49d0c537 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
DELETE FROM t1 WHERE f1=2
/*!*/;
# at 2134
#241223  9:07:14 server id 1  end_log_pos 2165 CRC32 0x5db7d8fa 	Xid = 64
COMMIT/*!*/;
# at 2165
#241223  9:07:14 server id 1  end_log_pos 2244 CRC32 0x6a5b2824 	Anonymous_GTID	last_committed=7	sequence_number=8	rbr_only=yes	original_committed_timestamp=1734934034080311	immediate_commit_timestamp=1734934034080311	transaction_length=290
/*!50718 SET TRANSACTION ISOLATION LEVEL READ COMMITTED*//*!*/;
# original_commit_timestamp=1734934034080311 (2024-12-23 09:07:14.080311 GMT)
# immediate_commit_timestamp=1734934034080311 (2024-12-23 09:07:14.080311 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034080311*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 2244
#241223  9:07:14 server id 1  end_log_pos 2330 CRC32 0xde21005f 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 2330
#241223  9:07:14 server id 1  end_log_pos 2378 CRC32 0x5e2d152a 	Table_map: `test`.`t1` mapped to number 161
# has_generated_invisible_primary_key=0
# at 2378
#241223  9:07:14 server id 1  end_log_pos 2424 CRC32 0xa7ac1272 	Update_rows: table id 161 flags: STMT_END_F

BINLOG '
Ev5oZxMBAAAAMAAAAEoJAAAAAKEAAAAAAAMABHRlc3QAAnQxAAEDAAABAQAqFS1e
Ev5oZx8BAAAALgAAAHgJAAAAAKEAAAAAAAEAAgAB//8AAwAAAAAJAAAAchKspw==
'/*!*/;
# at 2424
#241223  9:07:14 server id 1  end_log_pos 2455 CRC32 0xe06ea2fa 	Xid = 67
COMMIT/*!*/;
# at 2455
#241223  9:07:14 server id 1  end_log_pos 2534 CRC32 0xea804283 	Anonymous_GTID	last_committed=8	sequence_number=9	rbr_only=no	original_committed_timestamp=1734934034088706	immediate_commit_timestamp=1734934034088706	transaction_length=284
# original_commit_timestamp=1734934034088706 (2024-12-23 09:07:14.088706 GMT)
# immediate_commit_timestamp=1734934034088706 (2024-12-23 09:07:14.088706 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034088706*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 2534
#241223  9:07:14 server id 1  end_log_pos 2611 CRC32 0x48e4490e 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 2611
#241223  9:07:14 server id 1  end_log_pos 2708 CRC32 0xa3689a46 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
DELETE FROM t1 WHERE f1=4
/*!*/;
# at 2708
#241223  9:07:14 server id 1  end_log_pos 2739 CRC32 0xc18c3793 	Xid = 70
COMMIT/*!*/;
# at 2739
#241223  9:07:14 server id 1  end_log_pos 2818 CRC32 0x91946396 	Anonymous_GTID	last_committed=9	sequence_number=10	rbr_only=yes	original_committed_timestamp=1734934034096274	immediate_commit_timestamp=1734934034096274	transaction_length=290
/*!50718 SET TRANSACTION ISOLATION LEVEL READ COMMITTED*//*!*/;
# original_commit_timestamp=1734934034096274 (2024-12-23 09:07:14.096274 GMT)
# immediate_commit_timestamp=1734934034096274 (2024-12-23 09:07:14.096274 GMT)
/*!80001 SET @@session.original_commit_timestamp=1734934034096274*//*!*/;
/*!80014 SET @@session.original_server_version=90000*//*!*/;
/*!80014 SET @@session.immediate_server_version=90000*//*!*/;
SET @@SESSION.GTID_NEXT= 'ANONYMOUS'/*!*/;
# at 2818
#241223  9:07:14 server id 1  end_log_pos 2904 CRC32 0x91d62b8a 	Query	thread_id=9	exec_time=0	error_code=0
SET TIMESTAMP=1734934034/*!*/;
BEGIN
/*!*/;
# at 2904
#241223  9:07:14 server id 1  end_log_pos 2952 CRC32 0x2a0b4bde 	Table_map: `test`.`t1` mapped to number 161
# has_generated_invisible_primary_key=0
# at 2952
#241223  9:07:14 server id 1  end_log_pos 2998 CRC32 0x6106686e 	Update_rows: table id 161 flags: STMT_END_F

BINLOG '
Ev5oZxMBAAAAMAAAAIgLAAAAAKEAAAAAAAMABHRlc3QAAnQxAAEDAAABAQDeSwsq
Ev5oZx8BAAAALgAAALYLAAAAAKEAAAAAAAEAAgAB//8ABQAAAAAKAAAAbmgGYQ==
'/*!*/;
# at 2998
#241223  9:07:14 server id 1  end_log_pos 3029 CRC32 0xb1253825 	Xid = 73
COMMIT/*!*/;
# at 3029
#241223  9:07:14 server id 1  end_log_pos 3073 CRC32 0x33b62c1c 	Rotate to binlog.000002  pos: 4
SET debug="-d,fk";
SET @@SESSION.GTID_NEXT= 'AUTOMATIC' /* added by mysqlbinlog */ /*!*/;
DELIMITER ;
# End of log file
/*!50003 SET COMPLETION_TYPE=@OLD_COMPLETION_TYPE*/;
/*!50530 SET @@SESSION.PSEUDO_SLAVE_MODE=0*/;
