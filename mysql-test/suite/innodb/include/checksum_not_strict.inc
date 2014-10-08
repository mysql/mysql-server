if (`SELECT @@GLOBAL.innodb_checksum_algorithm like 'strict_%'`)
{
 --skip This test cannot be run when innodb_checksum_algorithm is in strict mode
}

