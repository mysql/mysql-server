CREATE TABLE t(j1 JSON);
SET DEBUG_SYNC = 'innodb_inplace_alter_table_enter SIGNAL start_dml WAIT_FOR alter_continue';
ALTER TABLE t row_format = compact;;
SET DEBUG_SYNC = 'now WAIT_FOR start_dml';
start transaction;
# Sven
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Sven"
UPDATE t SET j1 = JSON_SET(j1, '$[0]', 'Knut');
# Knut
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Knut"
UPDATE t SET j1 = JSON_SET(j1, '$[0]', 'Anna');
# Anna
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Anna"
rollback;
SET DEBUG_SYNC = 'now SIGNAL alter_continue';
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Sven"
drop table t;
SET DEBUG_SYNC = reset;
