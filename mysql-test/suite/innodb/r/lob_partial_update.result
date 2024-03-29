SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET GLOBAL log_error_verbosity=3;
set global innodb_compression_level = 0;
set debug = '+d,lob_print_partial_update_hit';
CREATE TABLE t(j1 JSON);
set @data_1 = repeat('abcdefghijklmnopqrstuvwxyz1234', 6006);
set @json_doc_1 = concat('["Sven", "', @data_1, '"]');
INSERT INTO t VALUES (@json_doc_1);
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
savepoint a;
UPDATE t SET j1 = JSON_SET(j1, '$[0]', 'Anna');
# Anna
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Anna"
rollback to a;
# Knut
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Knut"
commit;
# Knut
select json_extract(j1, '$[0]') from t;
json_extract(j1, '$[0]')
"Knut"
Pattern "LOB partial update of field=\(j1\) on index=\(\`GEN_CLUST_INDEX\`\) in table=\(test/t\)" found
drop table t;
set debug = '-d,lob_print_partial_update_hit';
set debug = '+d,zlob_print_partial_update_hit';
CREATE TABLE t2(j1 JSON) row_format=compressed;
INSERT INTO t2 VALUES (@json_doc_1);
start transaction;
# Sven
select json_extract(j1, '$[0]') from t2;
json_extract(j1, '$[0]')
"Sven"
UPDATE t2 SET j1 = JSON_SET(j1, '$[0]', 'Knut');
# Knut
select json_extract(j1, '$[0]') from t2;
json_extract(j1, '$[0]')
"Knut"
savepoint a;
UPDATE t2 SET j1 = JSON_SET(j1, '$[0]', 'Anna');
# Anna
select json_extract(j1, '$[0]') from t2;
json_extract(j1, '$[0]')
"Anna"
rollback to a;
# Knut
select json_extract(j1, '$[0]') from t2;
json_extract(j1, '$[0]')
"Knut"
commit;
# Knut
select json_extract(j1, '$[0]') from t2;
json_extract(j1, '$[0]')
"Knut"
Pattern "ZLOB partial update of field=\(j1\) on index=\(\`GEN_CLUST_INDEX\`\) in table=\(test/t2\)" found
drop table t2;
set debug = '-d,zlob_print_partial_update_hit';
set global innodb_compression_level = default;
SET GLOBAL log_error_verbosity= @orig_log_error_verbosity;
