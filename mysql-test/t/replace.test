#
# Test of REPLACE with MyISAM and HEAP
#

--disable_warnings
drop table if exists t1;
--enable_warnings

CREATE TABLE t1 (
  gesuchnr int(11) DEFAULT '0' NOT NULL,
  benutzer_id int(11) DEFAULT '0' NOT NULL,
  PRIMARY KEY (gesuchnr,benutzer_id)
);

replace into t1 (gesuchnr,benutzer_id) values (2,1);
replace into t1 (gesuchnr,benutzer_id) values (1,1);
replace into t1 (gesuchnr,benutzer_id) values (1,1);
alter table t1 engine=heap;
replace into t1 (gesuchnr,benutzer_id) values (1,1);
drop table t1;

#
# Test when using replace on a key that has used up it's whole range
#
SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create table t1 (a tinyint not null auto_increment primary key, b char(20) default "default_value");
insert into t1 values (126,"first"),(63, "middle"),(0,"last");
--error ER_DUP_ENTRY
insert into t1 values (0,"error");
--error ER_DUP_ENTRY
replace into t1 values (0,"error");
replace into t1 values (126,"first updated");
replace into t1 values (63,default);
select * from t1;
drop table t1;
SET sql_mode = default;
# End of 4.1 tests

#
# Bug#19789: REPLACE was allowed for a VIEW with CHECK OPTION enabled.
#
CREATE TABLE t1 (f1 INT);
CREATE VIEW v1 AS SELECT f1 FROM t1 WHERE f1 = 0 WITH CHECK OPTION;
--error 1369
REPLACE INTO v1 (f1) VALUES (1);
DROP TABLE t1;
DROP VIEW v1;
