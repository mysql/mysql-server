set @save_master_verify_checksum =  @@global.master_verify_checksum;
select @@global.master_verify_checksum  as 'must be zero because of default';
must be zero because of default
0
select @@session.master_verify_checksum  as 'no session var';
ERROR HY000: Variable 'master_verify_checksum' is a GLOBAL variable
set @@global.master_verify_checksum = 0;
set @@global.master_verify_checksum = default;
set @@global.master_verify_checksum = 2;
ERROR 42000: Variable 'master_verify_checksum' can't be set to the value of '2'
set @@global.master_verify_checksum = @save_master_verify_checksum;
