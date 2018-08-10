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
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "has_one_replicaset"
          },
          {
            "type": "LONGLONG",
            "name": "replicaset_is_ours"
          }
        ],
        "rows": [
          [
            1,
            1
          ]
        ]
      }
    },
    {
      "stmt": "SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "member_state"
          }
        ],
        "rows": [
          [
            "ONLINE"
          ]
        ]
      }
    },
    {
      "stmt": "SELECT SUM(IF(member_state = 'ONLINE', 1, 0)) as num_onlines, COUNT(*) as num_total FROM performance_schema.replication_group_members",
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "num_onlines"
          },
          {
            "type": "LONGLONG",
            "name": "num_total"
          }
        ],
        "rows": [
          [
            3,
            3
          ]
        ]
      }
    },
    {
      "stmt": "show status like 'ssl_cipher'",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "Variable_name"
          },
          {
            "type": "STRING",
            "name": "Value"
          }
        ],
        "rows": [
          [
            "Ssl_cipher",
            ""
          ]
        ]
      }
    },
    {
      "stmt": "SELECT F.cluster_name, R.replicaset_name, R.topology_type, JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) FROM mysql_innodb_cluster_metadata.clusters AS F, mysql_innodb_cluster_metadata.instances AS I, mysql_innodb_cluster_metadata.replicasets AS R WHERE R.replicaset_id = (SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances WHERE mysql_server_uuid = @@server_uuid)AND I.replicaset_id = R.replicaset_id AND R.cluster_id = F.cluster_id",
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "cluster_name"
          },
          {
            "type": "STRING",
            "name": "replicaset_name"
          },
          {
            "type": "STRING",
            "name": "topology_type"
          },
          {
            "type": "STRING",
            "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))"
          },
          {
            "type": "STRING",
            "name": "fake",
          }
        ],
        "rows": [
          [
            "test",
            "default",
            "pm",
            "localhost:5500",
            "x".repeat(4194304)
          ],
          [
            "test",
            "default",
            "pm",
            "localhost:5510",
            ""
          ],
          [
            "test",
            "default",
            "pm",
            "localhost:5520",
            ""
          ]
        ]
      }
    },
    {
      "stmt": "START TRANSACTION",
      "ok": {}
    },
    {
      "stmt.regex": "SELECT host_id, host_name, ip_address FROM mysql_innodb_cluster_metadata.hosts WHERE host_name = '.*' LIMIT 1",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "host_id"
          },
          {
            "type": "STRING",
            "name": "host_name"
          },
          {
            "type": "STRING",
            "name": "ip_address"
          }
        ]
      }
    },
    {
      "stmt.regex": "^INSERT INTO mysql_innodb_cluster_metadata.hosts.*",
      "ok": {
        "last_insert_id": 1
      }
    },
    {
      "stmt.regex": "^INSERT INTO mysql_innodb_cluster_metadata.routers.*",
      "ok": {
        "last_insert_id": 1
      }
    },



    // delete all old accounts if necessarry (ConfigGenerator::delete_account_for_all_hosts())
    {
      "stmt.regex": "^SELECT host FROM mysql.user WHERE user = '.*'",
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "COUNT..."
          }
        ],
        "rows": []  // to keep it simple, just tell Router there's no old accounts to erase
      }
    },


    // ConfigGenerator::create_account()
    {
      "stmt.regex": "^CREATE USER mysql_router1_[0-9a-z]{12}@'%' IDENTIFIED WITH mysql_native_password AS '\\*[0-9A-Z]{40}'",
      "ok": {}
    },
    {
      "stmt.regex": "^GRANT SELECT ON mysql_innodb_cluster_metadata.*",
      "ok": {}
    },
    {
      "stmt.regex": "^GRANT SELECT ON performance_schema.*",
      "ok": {}
    },
    {
      "stmt.regex": "^GRANT SELECT ON performance_schema.*",
      "ok": {}
    },



    {
      "stmt.regex": "^UPDATE mysql_innodb_cluster_metadata\\.routers.*",
      "ok": {}
    },
    {
      "stmt": "COMMIT",
      "ok": {}
    }
  ]
})
