/**
 * schema got created, but group replication isn't started.
 */

({
  "stmts": [
    {
      "stmt": "SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "major"
          },
          {
            "type": "LONGLONG",
            "name": "minor"
          },
          {
            "type": "LONGLONG",
            "name": "patch"
          }
        ],
        "rows": [
          [
            1,
            0,
            1
          ]
        ]
      }
    },
    {
      "stmt": "SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours",
      error: {
        code: 1193,
        message: "Unknown system variable 'group_replication_group_name'"
      }
    }
  ]
})
