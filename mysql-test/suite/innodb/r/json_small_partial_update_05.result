SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET GLOBAL log_error_verbosity=3;
CREATE TABLE t(f1 int, j1 JSON);
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
0	"Init00"
start transaction;
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
0	"Init00"
UPDATE t SET f1 = 1, j1 = JSON_SET(j1, '$[0]', 'Init01');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Init01"
UPDATE t SET f1 = 2, j1 = JSON_SET(j1, '$[0]', 'Init02');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
2	"Init02"
UPDATE t SET f1 = 3, j1 = JSON_SET(j1, '$[0]', 'Init03');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
3	"Init03"
UPDATE t SET f1 = 4, j1 = JSON_SET(j1, '$[0]', 'Init04');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
4	"Init04"
UPDATE t SET f1 = 5, j1 = JSON_SET(j1, '$[0]', 'Init05');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
5	"Init05"
UPDATE t SET f1 = 6, j1 = JSON_SET(j1, '$[0]', 'Init06');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
6	"Init06"
UPDATE t SET f1 = 7, j1 = JSON_SET(j1, '$[0]', 'Init07');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
7	"Init07"
UPDATE t SET f1 = 8, j1 = JSON_SET(j1, '$[0]', 'Init08');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
8	"Init08"
UPDATE t SET f1 = 9, j1 = JSON_SET(j1, '$[0]', 'Init09');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
9	"Init09"
UPDATE t SET f1 = 10, j1 = JSON_SET(j1, '$[0]', 'Init10');
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
10	"Init10"
rollback;
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
0	"Init00"
drop table t;
SET GLOBAL log_error_verbosity= @orig_log_error_verbosity;
