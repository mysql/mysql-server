######## Create Table Section #########
use test;

#dt TIMESTAMP, user CHAR(255), uuidf LONGBLOB,
eval CREATE TABLE test.regular_tbl(id INT NOT NULL AUTO_INCREMENT,
                           dt TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE
                           CURRENT_TIMESTAMP, user CHAR(255), uuidf VARBINARY(255),
                           fkid INT, filler VARCHAR(255),
                           PRIMARY KEY(id))
                           ENGINE=$engine_type;

eval CREATE TABLE test.byrange_tbl(id INT NOT NULL AUTO_INCREMENT,
                           dt TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE 
                           CURRENT_TIMESTAMP, user CHAR(255), uuidf VARBINARY(255),
                           fkid INT, filler VARCHAR(255),
                           PRIMARY KEY(id))
                           ENGINE=$engine_type
                                PARTITION BY RANGE(id)
                                (PARTITION pa100 values less than (100),
                                 PARTITION paMax values less than MAXVALUE);

######## Create SPs, Functions, Views and Triggers Section ##############

delimiter |;
CREATE PROCEDURE test.proc_norm()
BEGIN
   DECLARE ins_count INT DEFAULT 99;
   DECLARE cur_user VARCHAR(255);
   DECLARE local_uuid VARCHAR(255);

   SET cur_user= "current_user@localhost";
   SET local_uuid= "36774b1c-6374-11df-a2ca-0ef7ac7a5f6c";

   WHILE ins_count > 0 DO
     # Must use local variables for statment based replication
     INSERT INTO test.regular_tbl VALUES (NULL, NOW(), cur_user, local_uuid,
                                   ins_count,'Non partitioned table! Going to test replication for MySQL');
     SET ins_count = ins_count - 1;
   END WHILE;

END|

CREATE PROCEDURE test.proc_byrange()
BEGIN
   DECLARE ins_count INT DEFAULT 200;
   DECLARE cur_user VARCHAR(255);
   DECLARE local_uuid VARCHAR(255);

   SET cur_user= "current_user@localhost";
   SET local_uuid= "36774b1c-6374-11df-a2ca-0ef7ac7a5f6c";

   WHILE ins_count > 0 DO
     INSERT INTO test.byrange_tbl VALUES (NULL, NOW(), cur_user, local_uuid,
                                    ins_count + 100,'Partitioned table! Going to test replication for MySQL');
     SET ins_count = ins_count - 1;
   END WHILE;

END|

delimiter ;|

############ Finish Setup Section ###################


############ Test Section ###################

CALL test.proc_norm();
SELECT count(*) as "Master regular" FROM test.regular_tbl;
CALL test.proc_byrange();
SELECT count(*) as "Master byrange" FROM test.byrange_tbl;
show create table test.byrange_tbl;
show create table test.regular_tbl;
ALTER TABLE test.byrange_tbl EXCHANGE PARTITION pa100 WITH TABLE test.regular_tbl;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.byrange_tbl ORDER BY fkid LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.byrange_tbl ORDER BY fkid DESC LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.regular_tbl ORDER BY fkid LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.regular_tbl ORDER BY fkid DESC LIMIT 2;

--source include/sync_slave_sql_with_master.inc
connection slave;
show create table test.byrange_tbl;
show create table test.regular_tbl;
SELECT count(*) "Slave norm" FROM test.regular_tbl;
SELECT count(*) "Slave byrange" FROM test.byrange_tbl;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.byrange_tbl ORDER BY fkid LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.byrange_tbl ORDER BY fkid DESC LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.regular_tbl ORDER BY fkid LIMIT 2;
--replace_column 2 date-time 3 USER 4 UUID
SELECT * FROM test.regular_tbl ORDER BY fkid DESC LIMIT 2;

###### CLEAN UP SECTION ##############

connection master;
DROP PROCEDURE test.proc_norm;
DROP PROCEDURE test.proc_byrange;
DROP TABLE test.regular_tbl;
DROP TABLE test.byrange_tbl;
