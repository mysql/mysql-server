#Do not run the test when zstd lib has ver. >= 1.4.0

if (`SELECT min_value!=max_value FROM performance_schema.variables_info WHERE variable_name = 'mysqlx_zstd_default_compression_level'`) {
  skip Need zstd lib older than 1.4.0;
}
