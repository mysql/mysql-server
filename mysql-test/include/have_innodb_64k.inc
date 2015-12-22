if (`SELECT COUNT(*) = 0 FROM performance_schema.global_status WHERE LOWER(variable_name) = 'innodb_page_size' AND variable_value = 65536`)
{
  --skip Test requires InnoDB with 64k page size.
}
