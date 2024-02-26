SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET GLOBAL log_error_verbosity=3;
set global innodb_compression_level = 0;
CREATE TABLE tab82 (col28 JSON) row_format=compressed;
set @json_doc_1 = concat('["OpenSource", "', repeat('Linux+Ubuntu+', 16000), '"]');
set @json_doc_2 = concat('["OpenSource", "', repeat('MySQL+Oracle+', 16000), '"]');
set @data_1 = repeat('Linux+Ubuntu+', 16000);
set @data_2 = repeat('MySQL+Oracle+', 16000);
INSERT INTO tab82 VALUES (@json_doc_1);
# Connection default:
# Linux+Ubuntu
start transaction with consistent snapshot;
select strcmp(col28, @json_doc_1) = 0 from tab82;
strcmp(col28, @json_doc_1) = 0
1
select cast(col28 as char) = @json_doc_1 from tab82;
cast(col28 as char) = @json_doc_1
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_1 from tab82;
json_extract(col28, '$[1]') = @data_1
1
# Connection con1:
set @json_doc_1 = concat('["OpenSource", "', repeat('Linux+Ubuntu+', 16000), '"]');
set @json_doc_2 = concat('["OpenSource", "', repeat('MySQL+Oracle+', 16000), '"]');
set @data_1 = repeat('Linux+Ubuntu+', 16000);
set @data_2 = repeat('MySQL+Oracle+', 16000);
set debug = '+d,zlob_print_partial_update_hit';
start transaction;
UPDATE tab82 SET col28 = JSON_SET(col28, '$[1]', @data_2);
# MySQL+Oracle
select strcmp(col28, @json_doc_2) = 0 from tab82;
strcmp(col28, @json_doc_2) = 0
1
select cast(col28 as char) = @json_doc_2 from tab82;
cast(col28 as char) = @json_doc_2
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_2 from tab82;
json_extract(col28, '$[1]') = @data_2
1
# Connection default:
# Linux+Ubuntu
select strcmp(col28, @json_doc_1) = 0 from tab82;
strcmp(col28, @json_doc_1) = 0
1
select cast(col28 as char) = @json_doc_1 from tab82;
cast(col28 as char) = @json_doc_1
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_1 from tab82;
json_extract(col28, '$[1]') = @data_1
1
# Connection con1:
# MySQL+Oracle
select strcmp(col28, @json_doc_2) = 0 from tab82;
strcmp(col28, @json_doc_2) = 0
1
select cast(col28 as char) = @json_doc_2 from tab82;
cast(col28 as char) = @json_doc_2
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_2 from tab82;
json_extract(col28, '$[1]') = @data_2
1
commit;
# Connection default:
# Linux+Ubuntu
select strcmp(col28, @json_doc_1) = 0 from tab82;
strcmp(col28, @json_doc_1) = 0
1
select cast(col28 as char) = @json_doc_1 from tab82;
cast(col28 as char) = @json_doc_1
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_1 from tab82;
json_extract(col28, '$[1]') = @data_1
1
commit;
# MySQL+Oracle
select strcmp(col28, @json_doc_2) = 0 from tab82;
strcmp(col28, @json_doc_2) = 0
1
select cast(col28 as char) = @json_doc_2 from tab82;
cast(col28 as char) = @json_doc_2
1
select length(col28) from tab82;
length(col28)
208018
select json_extract(col28, '$[1]') = @data_2 from tab82;
json_extract(col28, '$[1]') = @data_2
1
Pattern "ZLOB partial update of field=\(col28\) on index=\(\`GEN_CLUST_INDEX\`\) in table=\(test/tab82\)" found
drop table tab82;
set debug = '-d,zlob_print_partial_update_hit';
set global innodb_compression_level = default;
SET GLOBAL log_error_verbosity= @orig_log_error_verbosity;
