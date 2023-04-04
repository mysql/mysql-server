#
# Compare values from SHOW STATUS with P_S.ndb_replication_applier_status
#

#
#SHOW STATUS LIKE "%ndb_%";
#SELECT * FROM performance_schema.global_status
#                          WHERE VARIABLE_NAME LIKE "NDB_%";
#

--disable_query_log
CREATE TEMPORARY TABLE check_columns (
  id INT PRIMARY KEY AUTO_INCREMENT,
  name varchar(255)
);
INSERT INTO check_columns
  SELECT NULL, COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = "performance_schema" AND
      TABLE_NAME = "ndb_replication_applier_status" ORDER BY ORDINAL_POSITION;
#SELECT * FROM check_columns;

let $col = 1;
while ($col < `SELECT COUNT(*) FROM check_columns`) {
  let $value_name = `SELECT name FROM check_columns WHERE id = $col`;
  #echo value_name: $value_name;

  let $pfs_value = `SELECT $value_name
                       FROM performance_schema.ndb_replication_applier_status
                         WHERE CHANNEL_NAME =""`;

  if ($value_name == MAX_REPLICATED_EPOCH) {
     # All have same name except this one
     let $value_name = REPLICA_$value_name;
  }

  let $var_name = $value_name;
  if (`SELECT COUNT(*) FROM performance_schema.global_status
         WHERE VARIABLE_NAME LIKE "Ndb%_replica" AND
           VARIABLE_NAME LIKE "NDB_$var_name%"`) {
    # Some variables need to have the names suffixed with "_replica"
    # to avoid reading the "global global" value.
    let $suffix =_replica;
    let $var_name = $var_name$suffix;
  }
  #echo var_name: $var_name;
  let $status_value = `SELECT VARIABLE_VALUE FROM performance_schema.global_status
                          WHERE VARIABLE_NAME LIKE "NDB_$var_name"`;
  #echo pfs_value: $pfs_value;
  #echo status_value: $status_value;
  assert($pfs_value == $status_value);

  inc $col;
}

DROP TABLE check_columns;
--enable_query_log
