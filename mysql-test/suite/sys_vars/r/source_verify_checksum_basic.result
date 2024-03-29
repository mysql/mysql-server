set @save_source_verify_checksum =  @@global.source_verify_checksum;
select @@global.source_verify_checksum  as 'must be zero because of default';
must be zero because of default
0
select @@session.source_verify_checksum  as 'no session var';
ERROR HY000: Variable 'source_verify_checksum' is a GLOBAL variable
set @@global.source_verify_checksum = 0;
set @@global.source_verify_checksum = default;
set @@global.source_verify_checksum = 2;
ERROR 42000: Variable 'source_verify_checksum' can't be set to the value of '2'
set @@global.source_verify_checksum = @save_source_verify_checksum;
