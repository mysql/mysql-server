if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_STATUS WHERE LOWER(variable_name) = 'innodb_page_size' AND variable_value = 8192`)
{
  --skip Test requires InnoDB with 8k Page size.
}
