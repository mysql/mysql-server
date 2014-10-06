if (`select count(*) = 0 from information_schema.global_variables where variable_name like 'innodb_undo_tablespaces' and variable_value >= 2`)
{
  --skip Test requires InnoDB with at-least 2 undo tablespaces.
}
