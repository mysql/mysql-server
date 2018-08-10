({
    "stmts": [
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.061723,
            "ok": {}
        },
        {
            "stmt.regex": "^SELECT host_id, host_name, ip_address FROM mysql_innodb_cluster_metadata.hosts WHERE host_name = '.*' LIMIT 1",
            "exec_time": 0.237026,
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
                        "127.0.0.1",
                        null
                    ]
                ]
            }
        },
        {
            "stmt": "INSERT INTO mysql_innodb_cluster_metadata.routers        (host_id, router_name) VALUES (8, '')",
            "exec_time": 0.283136,
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
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_members TO mysql_router8_.*@'%'",
            "exec_time": 8.584342,
            "ok": {}
        },
        {
            "stmt.regex": "^GRANT SELECT ON performance_schema.replication_group_member_stats TO mysql_router8_.*@'%'",
            "exec_time": 6.240789,
            "ok": {}
        },



        {
            "stmt": "UPDATE mysql_innodb_cluster_metadata.routers SET attributes =    JSON_SET(JSON_SET(JSON_SET(JSON_SET(attributes,    'RWEndpoint', '6446'),    'ROEndpoint', '6447'),    'RWXEndpoint', '64460'),    'ROXEndpoint', '64470') WHERE router_id = 8",
            "exec_time": 0.277948,
            "ok": {}
        },
        {
            "stmt": "COMMIT",
            "exec_time": 0.090678,
            "ok": {}
        }
    ]
})
