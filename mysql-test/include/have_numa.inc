--let $_hn_unknown_var= convert_error(ER_UNKNOWN_SYSTEM_VARIABLE)
--error 0, $_hn_unknown_var
--let $_hn_innodb_numa_interleave= `SELECT @@GLOBAL.innodb_numa_interleave`
--let $_hn_error= query_get_value(SHOW ERRORS, Code, 1)
if ($_hn_error == $_hn_unknown_var)
{
  --skip Required NUMA support
}
