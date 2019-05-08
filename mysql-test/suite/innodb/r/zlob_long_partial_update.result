SET @orig_log_error_verbosity= @@GLOBAL.log_error_verbosity;
SET GLOBAL log_error_verbosity=3;
set global innodb_compression_level = 0;
set debug = '+d,zlob_print_partial_update_hit';
CREATE TABLE z2 (q123 JSON) engine=innodb row_format=compressed;
set @json_doc_1 = concat('["OpenSource", "', repeat('Linux+Ubuntu+', 16000), '"]');
set @json_doc_2 = concat('["OpenSource", "', repeat('MySQL+Oracle+', 16000), '"]');
set @data_1 = repeat('Linux+Ubuntu+', 16000);
set @data_2 = repeat('MySQL+Oracle+', 16000);
# Inserting...
INSERT INTO z2 VALUES ( @json_doc_1 );
# Linux+Ubuntu
select strcmp(q123, @json_doc_1) = 0 as f1 from z2;
f1
1
select cast(q123 as char) = @json_doc_1 as f1 from z2;
f1
1
select length(q123) from z2;
length(q123)
208018
select json_extract(q123, '$[1]') = @data_1 from z2;
json_extract(q123, '$[1]') = @data_1
1
# Updating...
UPDATE z2 SET q123 = JSON_SET(q123, '$[1]', repeat('MySQL+Oracle+', 16000));
# MySQL+Oracle
select strcmp(q123, @json_doc_2) = 0 as f1 from z2;
f1
1
select cast(q123 as char) = @json_doc_2 from z2;
cast(q123 as char) = @json_doc_2
1
select length(q123) from z2;
length(q123)
208018
select json_extract(q123, '$[1]') = @data_2 from z2;
json_extract(q123, '$[1]') = @data_2
1
Pattern "ZLOB partial update of field=\(q123\) on index=\(\`GEN_CLUST_INDEX\`\) in table=\(test/z2\)" found
drop table z2;
set debug = '-d,lob_print_partial_update_hit';
set global innodb_compression_level = default;
SET GLOBAL log_error_verbosity= @orig_log_error_verbosity;
