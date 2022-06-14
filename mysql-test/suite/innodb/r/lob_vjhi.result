create table t1 (f1 int primary key, j json, vjhi int as (json_unquote(j->'$.Year')) virtual);
show create table t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `f1` int NOT NULL,
  `j` json DEFAULT NULL,
  `vjhi` int GENERATED ALWAYS AS (json_unquote(json_extract(`j`,_utf8mb4'$.Year'))) VIRTUAL,
  PRIMARY KEY (`f1`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2009
# Connection default:
start transaction with consistent snapshot;
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2009
# Connection con1:
start transaction;
UPDATE t1 SET j = JSON_SET(j, '$.Year', '2010');
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2010
# Connection default:
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2009
# Connection con1:
commit;
# Connection default:
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2009
commit;
select f1, vjhi from t1 order by f1;
f1	vjhi
1	2010
drop table t1;
