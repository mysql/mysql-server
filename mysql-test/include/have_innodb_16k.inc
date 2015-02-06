--disable_warnings
if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE LOWER(variable_name) = 'innodb_page_size' AND variable_value = 16384`)
{
  --skip Test requires InnoDB with 16k Page size.
}
--enable_warnings
