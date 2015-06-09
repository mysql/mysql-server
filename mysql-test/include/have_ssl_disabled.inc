if (`SELECT COUNT(*) = 0 FROM INFORMATION_SCHEMA.GLOBAL_VARIABLES WHERE VARIABLE_NAME like 'have_ssl' and VARIABLE_VALUE like 'DISABLED'`)
{
  --skip Test requires ssl to be disabled.
}
