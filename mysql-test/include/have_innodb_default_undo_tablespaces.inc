if (`SELECT COUNT(*) != 2 FROM information_schema.innodb_tablespaces WHERE space_type = 'Undo'`)
{
  --skip Test requires only the default undo tablespaces
}
