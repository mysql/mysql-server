SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET GLOBAL log_error_verbosity=3;
CREATE TABLE t(f1 int, j1 JSON);
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
start transaction;
UPDATE t SET f1 = 2, j1 = JSON_SET(j1, '$[0]', 'Knut');
# Knut
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
2	"Knut"
rollback;
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
drop table t;
CREATE TABLE t(f1 int, j1 JSON) row_format=compact;
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
start transaction;
UPDATE t SET f1 = 2, j1 = JSON_SET(j1, '$[0]', 'Knut');
# Knut
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
2	"Knut"
rollback;
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
drop table t;
CREATE TABLE t(f1 int, j1 JSON) row_format=redundant;
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
start transaction;
UPDATE t SET f1 = 2, j1 = JSON_SET(j1, '$[0]', 'Knut');
# Knut
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
2	"Knut"
rollback;
# Sven
select f1, json_extract(j1, '$[0]') from t;
f1	json_extract(j1, '$[0]')
1	"Sven"
drop table t;
SET GLOBAL log_error_verbosity= @orig_log_error_verbosity;
