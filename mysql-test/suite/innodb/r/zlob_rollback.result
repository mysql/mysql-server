set global innodb_compression_level = 0;
CREATE TABLE t(j1 JSON) row_format = compressed;
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
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Sven"
drop table t;
set global innodb_compression_level = default;
