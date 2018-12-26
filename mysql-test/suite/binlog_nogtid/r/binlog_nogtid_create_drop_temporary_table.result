BEGIN;
CREATE TEMPORARY TABLE tmp1 (a INT) ENGINE=InnoDB;
DROP TEMPORARY TABLE tmp1;
CREATE TEMPORARY TABLE tmp2 (a INT) ENGINE=MyISAM;
DROP TEMPORARY TABLE tmp2;
COMMIT;
include/assert.inc [CREATE/DROP TEMPORARY TABLE is not written into binlog when @@session.binlog_format=ROW/MIXED and @@global.gtid_mode=OFF.]
