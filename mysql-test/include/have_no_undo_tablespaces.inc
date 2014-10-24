if (`select count(*) = 0 from information_schema.global_variables where variable_name like 'innodb_undo_tablespaces' and variable_value = 0`)
{
  --skip Test requires innodb_undo_tablespaces=0
}
