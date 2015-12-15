let $numa_support = `SELECT COUNT(VARIABLE_VALUE) = 1 FROM
  performance_schema.global_variables
  WHERE VARIABLE_NAME='innodb_numa_interleave'`;

if ( $numa_support == 0 )
{
    --skip Test requires: NUMA must be enabled
}

if ( $numa_support == 1 )
{
  let $numa = `SELECT @@GLOBAL.innodb_numa_interleave = 'OFF'`;
  if ( $numa == 1 )
  {
    --skip Test requires: NUMA must be enabled
  }
}

