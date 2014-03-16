if (!`SHOW VARIABLES LIKE 'innodb_change_buffering_debug'`)
{
  # innodb_change_buffering_debug is enabled by UNIV_DEBUG or
  # UNIV_IBUF_DEBUG
  --skip Test requires binary with UNIV_DEBUG enabled
}
