if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE LOWER(variable_name) = 'innodb_page_size' AND variable_value <= 16384`)
{
  --skip Test with InnoDB zip requires page size not greater than 16k.
}
