CREATE TABLE t(f1 int primary key, j1 JSON);
start transaction;
UPDATE t SET j1 = JSON_SET(j1, '$[0]', 'Knut');
delete from t;
commit;
drop table t;
