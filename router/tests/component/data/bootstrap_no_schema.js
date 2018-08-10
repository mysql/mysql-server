/**
 * schema got created, but group replication isn't started.
 */

({
  "stmts": [
    {
      "stmt": "SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
      error: {
        code: 1049,
        message: "Unknown database 'mysql_innodb_cluster_metadata'"
      }
    }
  ]
})
