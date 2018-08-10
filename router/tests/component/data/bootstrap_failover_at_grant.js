{
    "stmts": [
        {
            "stmt": "SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
            "exec_time": 0.232933,
            "result": {
                "columns": [
                    {
                        "name": "major",
                        "type": "LONG"
                    },
                    {
                        "name": "minor",
                        "type": "LONG"
                    },
                    {
                        "name": "patch",
                        "type": "LONG"
                    }
                ],
                "rows": [
                    [
                        "1",
                        "0",
                        "2"
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours",
            "exec_time": 0.424781,
            "result": {
                "columns": [
                    {
                        "name": "has_one_replicaset",
                        "type": "LONGLONG"
                    },
                    {
                        "name": "replicaset_is_ours",
                        "type": "LONGLONG"
                    }
                ],
                "rows": [
                    [
                        "1",
                        "1"
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid",
            "exec_time": 0.170235,
            "result": {
                "columns": [
                    {
                        "name": "member_state",
                        "type": "STRING"
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
            "exec_time": 0.151384,
            "result": {
                "columns": [
                    {
                        "name": "num_onlines",
                        "type": "NEWDECIMAL"
                    },
                    {
                        "name": "num_total",
                        "type": "LONGLONG"
                    }
                ],
                "rows": [
                    [
                        "3",
                        "3"
                    ]
                ]
            }
        },
        {
            "stmt": "show status like 'ssl_cipher'",
            "exec_time": 0.800325,
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
                        "Ssl_cipher",
                        ""
                    ]
                ]
            }
        },
        {
            "stmt": "SELECT F.cluster_name, R.replicaset_name, R.topology_type, JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) FROM mysql_innodb_cluster_metadata.clusters AS F, mysql_innodb_cluster_metadata.instances AS I, mysql_innodb_cluster_metadata.replicasets AS R WHERE R.replicaset_id = (SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances WHERE mysql_server_uuid = @@server_uuid)AND I.replicaset_id = R.replicaset_id AND R.cluster_id = F.cluster_id",
            "exec_time": 0.418633,
            "result": {
                "columns": [
                    {
                        "name": "cluster_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "replicaset_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "topology_type",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))",
                        "type": "LONGBLOB"
                    }
                ],
                "rows": [
                    [
                        process.env.MYSQL_SERVER_MOCK_CLUSTER_NAME,
                        "myreplicaset",
                        "pm",
                        "127.0.0.1:13001"
                    ],
                    [
                        process.env.MYSQL_SERVER_MOCK_CLUSTER_NAME,
                        "myreplicaset",
                        "pm",
                        "127.0.0.1:13002"
                    ],
                    [
                        process.env.MYSQL_SERVER_MOCK_CLUSTER_NAME,
                        "myreplicaset",
                        "pm",
                        "127.0.0.1:13003"
                    ]
                ]
            }
        },
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.082893,
            "ok": {}
        },
        {
            "stmt.regex": "^SELECT host_id, host_name, ip_address FROM mysql_innodb_cluster_metadata.hosts WHERE host_name = '..*' LIMIT 1",
            "exec_time": 0.296962,
            "result": {
                "columns": [
                    {
                        "name": "host_id",
                        "type": "LONG"
                    },
                    {
                        "name": "host_name",
                        "type": "VAR_STRING"
                    },
                    {
                        "name": "ip_address",
                        "type": "VAR_STRING"
                    }
                ],
                "rows": [
                    [
                        "8",
                        process.env.MYSQL_SERVER_MOCK_HOST_1,
                        null
                    ]
                ]
            }
        },
        {
            "stmt": "INSERT INTO mysql_innodb_cluster_metadata.routers        (host_id, router_name) VALUES (8, '')",
            "exec_time": 0.152557,
            "ok": {
              "last_insert_id": 8
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
            "stmt.regex": "^CREATE USER mysql_router8_[0-9a-z]{12}@'%' IDENTIFIED WITH mysql_native_password AS '\\*[0-9A-Z]{40}'",
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON mysql_innodb_cluster_metadata.* TO mysql_router8_.*@'%'",
            "exec_time": 8.536869,
            "error": { // here we trigger the failover
                "code": 1290,
                "message": "The MySQL server is running with the --super-read-only option so it cannot execute this statement",
                "sql_state": "HY000"
            }
        },
        {
            "stmt": "ROLLBACK",
            "exec_time": 0.100835,
            "ok": {}
        },
        {
            "stmt": "ROLLBACK",
            "exec_time": 0.100835,
            "ok": {}
        },
        {
            "stmt": "SELECT member_host, member_port   FROM performance_schema.replication_group_members  /*!80002 ORDER BY member_role */",
            "exec_time": 0.135675,
            "result": {
                "columns": [
                    {
                        "name": "member_host",
                        "type": "STRING"
                    },
                    {
                        "name": "member_port",
                        "type": "LONG"
                    }
                ],
                "rows": [
                    [
                        process.env.MYSQL_SERVER_MOCK_HOST_1,
                        process.env.MYSQL_SERVER_MOCK_PORT_1
                    ],
                    [
                        process.env.MYSQL_SERVER_MOCK_HOST_2,
                        process.env.MYSQL_SERVER_MOCK_PORT_2
                    ],
                    [
                        process.env.MYSQL_SERVER_MOCK_HOST_3,
                        process.env.MYSQL_SERVER_MOCK_PORT_3
                    ]
                ]
            }
        }
    ]
}
