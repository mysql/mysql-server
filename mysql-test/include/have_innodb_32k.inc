if (`SELECT COUNT(*) = 0 FROM performance_schema.global_status WHERE LOWER(variable_name) = 'innodb_page_size' AND variable_value = 32768`)
{
  --skip Test requires InnoDB with 32k page size.
}
