RESET MASTER;
CREATE TABLE t1 (id bigint NOT NULL AUTO_INCREMENT PRIMARY KEY,
x mediumblob NOT NULL) ENGINE=InnoDB;
PREPARE s FROM "INSERT INTO t1 (x) VALUES (?)";
SET @a = LOAD_FILE('../../std_data/binlog_compression.gz');
EXECUTE s USING @a;
include/assert_binlog_events.inc [!Gtid_or_anon # !DDL # !Gtid_or_anon # Transaction_payload # !Begin # Table_map # Write_rows # Xid]
DROP TABLE t1;
