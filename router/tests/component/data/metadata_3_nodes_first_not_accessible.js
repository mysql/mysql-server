{
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
            "stmt": "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';",
            "exec_time": 1.862005,
            "result": {
                "columns": [
                    {
                        "name": "replicaset_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "mysql_server_uuid",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "role",
                        "type": "STRING"
                    },
                    {
                        "name": "weight",
                        "type": "FLOAT"
                    },
                    {
                        "name": "version_token",
                        "type": "LONG"
                    },
                    {
                        "name": "location",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlClassic'",
                        "type": "LONGBLOB"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlX'",
                        "type": "LONGBLOB"
                    }
                ],
                "rows": [
                    [
                        "default",
                        "8502d47b-e0f2-11e7-8530-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.PRIMARY_HOST,
                        "localhost:33100"
                    ],
                    [
                        "default",
                        "8986ad3f-e0f2-11e7-875e-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_1_HOST,
                        "127.0.0.1:33110"
                    ],
                    [
                        "default",
                        "8dfc649f-e0f2-11e7-8aa8-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_2_HOST,
                        "127.0.0.1:33120"
                    ]
                ]
            }
        },
        {
            "stmt": "show status like 'group_replication_primary_member'",
            "exec_time": 3.374584,
            "result": {
                "columns": [
                    {
                        "name": "Variable_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "Value",
                        "type": "VAR_STRING"
                    }
                ],
                "rows": [
                    [
                        "group_replication_primary_member",
                        "8986ad3f-e0f2-11e7-875e-080027c84a43"
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'",
            "exec_time": 1.422041,
            "result": {
                "columns": [
                    {
                        "name": "member_id",
                        "type": "STRING"
                    },
                    {
                        "name": "member_host",
                        "type": "STRING"
                    },
                    {
                        "name": "member_port",
                        "type": "LONG"
                    },
                    {
                        "name": "member_state",
                        "type": "STRING"
                    },
                    {
                        "name": "@@group_replication_single_primary_mode",
                        "type": "LONGLONG"
                    }
                ],
                "rows": [
                    [
                        "8986ad3f-e0f2-11e7-875e-080027c84a43",
                        "jacek-VirtualBox",
                        process.env.SECONDARY_1_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "8dfc649f-e0f2-11e7-8aa8-080027c84a43",
                        "jacek-VirtualBox",
                        process.env.SECONDARY_2_PORT,
                        "ONLINE",
                        "1"
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = 'test';",
            "exec_time": 0.521481,
            "result": {
                "columns": [
                    {
                        "name": "replicaset_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "mysql_server_uuid",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "role",
                        "type": "STRING"
                    },
                    {
                        "name": "weight",
                        "type": "FLOAT"
                    },
                    {
                        "name": "version_token",
                        "type": "LONG"
                    },
                    {
                        "name": "location",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlClassic'",
                        "type": "LONGBLOB"
                    },
                    {
                        "name": "I.addresses->>'$.mysqlX'",
                        "type": "LONGBLOB"
                    }
                ],
                "rows": [
                    [
                        "default",
                        "8502d47b-e0f2-11e7-8530-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.PRIMARY_HOST,
                        "localhost:33100"
                    ],
                    [
                        "default",
                        "8986ad3f-e0f2-11e7-875e-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_1_HOST,
                        "127.0.0.1:33110"
                    ],
                    [
                        "default",
                        "8dfc649f-e0f2-11e7-8aa8-080027c84a43",
                        "HA",
                        null,
                        null,
                        "",
                        process.env.SECONDARY_2_HOST,
                        "127.0.0.1:33120"
                    ]
                ]
            }
        },
        {
            "stmt": "show status like 'group_replication_primary_member'",
            "exec_time": 1.702137,
            "result": {
                "columns": [
                    {
                        "name": "Variable_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "Value",
                        "type": "VAR_STRING"
                    }
                ],
                "rows": [
                    [
                        "group_replication_primary_member",
                        "8986ad3f-e0f2-11e7-875e-080027c84a43"
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'",
            "exec_time": 1.187367,
            "result": {
                "columns": [
                    {
                        "name": "member_id",
                        "type": "STRING"
                    },
                    {
                        "name": "member_host",
                        "type": "STRING"
                    },
                    {
                        "name": "member_port",
                        "type": "LONG"
                    },
                    {
                        "name": "member_state",
                        "type": "STRING"
                    },
                    {
                        "name": "@@group_replication_single_primary_mode",
                        "type": "LONGLONG"
                    }
                ],
                "rows": [
                    [
                        "8986ad3f-e0f2-11e7-875e-080027c84a43",
                        "jacek-VirtualBox",
                        process.env.SECONDARY_1_PORT,
                        "ONLINE",
                        "1"
                    ],
                    [
                        "8dfc649f-e0f2-11e7-8aa8-080027c84a43",
                        "jacek-VirtualBox",
                        process.env.SECONDARY_2_PORT,
                        "ONLINE",
                        "1"
                    ]
                ]
            }
        }
    ]
}
