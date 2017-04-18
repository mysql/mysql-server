
CREATE USER test1@localhost;
CREATE USER test2@localhost;
GRANT USAGE ON *.* TO test1@localhost;
GRANT USAGE ON *.* TO test2@localhost;
GRANT CREATE, DROP, INSERT, SELECT ON test.* TO test1@localhost;
GRANT CREATE, DROP, ALTER, UPDATE, INSERT, SELECT ON test.* TO test2@localhost;

--echo connect (test1,localhost,test1,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test1,localhost,test1,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
SELECT current_user();
SHOW GRANTS FOR CURRENT_USER;
--disable_result_log
--disable_query_log
--source suite/parts/inc/part_exch_tabs.inc
--enable_result_log
--enable_query_log
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--echo disconnect test1;
disconnect test1;


--echo connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test2,localhost,test2,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
USE test;
SELECT current_user();
SHOW GRANTS FOR CURRENT_USER;

# 9) Exchanges with different owner.
# Privilege for ALTER and SELECT
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to former values.
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
--sorted_result
SELECT * FROM tsp_00;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
# Back to former values.
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
--sorted_result
SELECT * FROM tsp_00;
--sorted_result
SELECT * FROM tsp WHERE a BETWEEN 0 AND 10;
--echo disconnect test2;
disconnect test2;

--echo connection default;
connection default;
REVOKE ALTER ON test.* FROM test2@localhost;

--echo connect (test2,localhost,test2,,test,MASTER_MYPORT,MASTER_MYSOCK);
connect (test2,localhost,test2,,test,$MASTER_MYPORT,$MASTER_MYSOCK);
USE test;
SELECT current_user();
SHOW GRANTS FOR CURRENT_USER;
# Privilege for ALTER and SELECT
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
SELECT * FROM tp WHERE a BETWEEN 0 AND 10;
# Back to former values.
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tp EXCHANGE PARTITION p0 WITH TABLE t_10;
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;
# Back to former values.
--error ER_TABLEACCESS_DENIED_ERROR
ALTER TABLE tsp EXCHANGE PARTITION sp00 WITH TABLE tsp_00;

--echo connection default;
connection default;
--echo disconnect test2;
disconnect test2;

--source suite/parts/inc/part_exch_drop_tabs.inc

DROP USER test1@localhost;
DROP USER test2@localhost;

