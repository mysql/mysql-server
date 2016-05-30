let $numa_support = `SELECT COUNT(VARIABLE_VALUE) = 1 FROM
  performance_schema.global_variables
  WHERE VARIABLE_NAME='innodb_numa_interleave'`;

if ( $numa_support == 0 )
{
    --skip Test requires: Binary must be built with NUMA support.
}

