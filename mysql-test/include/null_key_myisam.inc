#
# Check null keys

# test needs MyISAM storage engine
--source include/force_myisam_default.inc
--source include/have_myisam.inc

SET sql_mode = 'NO_ENGINE_SUBSTITUTION';
create table t1 (a int, b int not null,unique key (a,b),index(b)) engine=myisam;
insert ignore into t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(null,7),(9,9),(8,8),(7,7),(null,9),(null,9),(6,6);
explain select * from t1 where a is null;
explain select * from t1 where a is null and b = 2;
explain select * from t1 where a is null and b = 7;
explain select * from t1 where a=2 and b = 2;
explain select * from t1 where a<=>b limit 2;
explain select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
--skip_if_hypergraph  # Depends on the query plan.
explain format=tree select * from t1 where (a is null or a = 7) and b=7;
explain select * from t1 where (a is null or a = 7) and b=7;
explain select * from t1 where (a is null or a = 7) and b=7 order by a;
explain select * from t1 where (a is null and b>a) or a is null and b=7 limit 2;
explain select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
explain select * from t1 where a > 1 and a < 3 limit 1;
explain select * from t1 where a > 8 and a < 9;
--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is null and b = 7;
--sorted_result
select * from t1 where a<=>b limit 2;
--sorted_result
select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
--sorted_result
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;
--sorted_result
select * from t1 where (a is null or a = 7) and b=7;
--sorted_result
select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
--sorted_result
select * from t1 where a > 1 and a < 3 limit 1;
--sorted_result
select * from t1 where a > 8 and a < 9;
create table t2 like t1;
insert into t2 select * from t1;
alter table t1 modify b blob not null, add c int not null, drop key a, add unique key (a,b(20),c), drop key b, add key (b(10));
explain select * from t1 where a is null and b = 2;
explain select * from t1 where a is null and b = 2 and c=0;
explain select * from t1 where a is null and b = 7 and c=0;
explain select * from t1 where a=2 and b = 2;
explain select * from t1 where a<=>b limit 2;
explain select * from t1 where (a is null or a > 0 and a < 3) and b < 5 and c=0 limit 3;
explain select * from t1 where (a is null or a = 7) and b=7 and c=0;
explain select * from t1 where (a is null and b>a) or a is null and b=7 limit 2;
explain select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
explain select * from t1 where a > 1 and a < 3 limit 1;
explain select * from t1 where a is null and b=7 or a > 1 and a < 3 limit 1;
explain select * from t1 where a > 8 and a < 9;
explain select * from t1 where b like "6%";
--sorted_result
select * from t1 where a is null;
--sorted_result
select * from t1 where a is null and b = 7 and c=0;
--sorted_result
select * from t1 where a<=>b limit 2;
--sorted_result
select * from t1 where (a is null or a > 0 and a < 3) and b < 5 limit 3;
--sorted_result
select * from t1 where (a is null or a > 0 and a < 3) and b > 7 limit 3;
--sorted_result
select * from t1 where (a is null or a = 7) and b=7 and c=0;
--sorted_result
select * from t1 where a is null and b=9 or a is null and b=7 limit 3;
--sorted_result
select * from t1 where b like "6%";

#
# Test ref_or_null optimization
#
drop table t1;
rename table t2 to t1;
alter table t1 modify b int null;
insert into t1 values (7,null), (8,null), (8,7);
explain select * from t1 where a = 7 and (b=7 or b is null);
--sorted_result
select * from t1 where a = 7 and (b=7 or b is null);
explain select * from t1 where (a = 7 or a is null) and (b=7 or b is null);
--sorted_result
select * from t1 where (a = 7 or a is null) and (b=7 or b is null);
explain select * from t1 where (a = 7 or a is null) and (a = 7 or a is null);
--sorted_result
select * from t1 where (a = 7 or a is null) and (a = 7 or a is null);
create table t2 (a int);
insert into t2 values (7),(8);
explain select * from t2 straight_join t1 where t1.a=t2.a and b is null;
drop index b on t1;
explain select * from t2,t1 where t1.a=t2.a and b is null;
--sorted_result
select * from t2,t1 where t1.a=t2.a and b is null;
explain select * from t2,t1 where t1.a=t2.a and (b= 7 or b is null);
--sorted_result
select * from t2,t1 where t1.a=t2.a and (b= 7 or b is null);
explain select * from t2,t1 where (t1.a=t2.a or t1.a is null) and b= 7;
--sorted_result
select * from t2,t1 where (t1.a=t2.a or t1.a is null) and b= 7;
explain select * from t2,t1 where (t1.a=t2.a or t1.a is null) and (b= 7 or b is null);
--sorted_result
select * from t2,t1 where (t1.a=t2.a or t1.a is null) and (b= 7 or b is null);
insert into t2 values (null),(6);
delete from t1 where a=8;
explain select * from t2,t1 where t1.a=t2.a or t1.a is null;
explain select * from t2,t1 where t1.a<=>t2.a or (t1.a is null and t1.b <> 9);
--sorted_result
select * from t2,t1 where t1.a<=>t2.a or (t1.a is null and t1.b <> 9);
drop table t1,t2;

#
# The following failed for Matt Loschert
#

CREATE TABLE t1 (
  id int(10) unsigned NOT NULL auto_increment,
  uniq_id int(10) unsigned default NULL,
  PRIMARY KEY  (id),
  UNIQUE KEY idx1 (uniq_id)
) ENGINE=MyISAM;

CREATE TABLE t2 (
  id int(10) unsigned NOT NULL auto_increment,
  uniq_id int(10) unsigned default NULL,
  PRIMARY KEY  (id)
) ENGINE=MyISAM;

INSERT INTO t1 VALUES (1,NULL),(2,NULL),(3,1),(4,2),(5,NULL),(6,NULL),(7,3),(8,4),(9,NULL),(10,NULL);
INSERT INTO t2 VALUES (1,NULL),(2,NULL),(3,1),(4,2),(5,NULL),(6,NULL),(7,3),(8,4),(9,NULL),(10,NULL);

#
# Check IS NULL optimization
#
explain select id from t1 where uniq_id is null;
explain select id from t1 where uniq_id =1;
#
# Check updates
#
UPDATE t1 SET id=id+100 where uniq_id is null;
UPDATE t2 SET id=id+100 where uniq_id is null;
select id from t1 where uniq_id is null;
select id from t2 where uniq_id is null;
#
# Delete all records from each table where the uniq_id field is null
#
DELETE FROM t1 WHERE uniq_id IS NULL;
DELETE FROM t2 WHERE uniq_id IS NULL;
#
# Select what is left -- notice the difference
#
SELECT * FROM t1 ORDER BY uniq_id, id;
SELECT * FROM t2 ORDER BY uniq_id, id;
DROP table t1,t2;

#
# This crashed MySQL 3.23.47
#

CREATE TABLE `t1` (
  `order_id` char(32) NOT NULL default '',
  `product_id` char(32) NOT NULL default '',
  `product_type` int(11) NOT NULL default '0',
  PRIMARY KEY  (`order_id`,`product_id`,`product_type`)
) ENGINE=MyISAM;
CREATE TABLE `t2` (
  `order_id` char(32) NOT NULL default '',
  `product_id` char(32) NOT NULL default '',
  `product_type` int(11) NOT NULL default '0',
  PRIMARY KEY  (`order_id`,`product_id`,`product_type`)
) ENGINE=MyISAM;
INSERT INTO t1 (order_id, product_id, product_type) VALUES
('3d7ce39b5d4b3e3d22aaafe9b633de51',1206029, 3),
('3d7ce39b5d4b3e3d22aaafe9b633de51',5880836, 3),
('9d9aad7764b5b2c53004348ef8d34500',2315652, 3);
INSERT INTO t2 (order_id, product_id, product_type) VALUES
('9d9aad7764b5b2c53004348ef8d34500',2315652, 3);

select t1.* from t1
left join t2 using(order_id, product_id, product_type)
where t2.order_id=NULL;
select t1.* from t1
left join t2 using(order_id, product_id, product_type)
where t2.order_id is NULL;
drop table t1,t2;

SET sql_mode = default;

#
# Test that EXPLAIN format=tree prints "or NULL" on the keypart that
# is actually nullable, not always the first one.
#
create table t1 (a int, b int not null,unique key (b,a),index(b));
insert ignore into t1 values (1,1),(2,2),(3,3),(4,4),(5,5),(6,6),(null,7),(9,9),(8,8),(7,7),(null,9),(null,9),(6,6);
--skip_if_hypergraph  # Depends on the query plan.
explain format=tree select * from t1 where (a is null or a = 7) and b=7;
drop table t1;
