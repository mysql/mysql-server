{
    "stmts": [
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.057513,
            "ok": {}
        },
        {
            "stmt": "SELECT h.host_id, h.host_name FROM mysql_innodb_cluster_metadata.routers r JOIN mysql_innodb_cluster_metadata.hosts h    ON r.host_id = h.host_id WHERE r.router_id = 8",
            "exec_time": 0.175663,
            "result": {
                "columns": [
                    {
                        "name": "host_id",
                        "type": "LONG"
                    },
                    {
                        "name": "host_name",
                        "type": "VAR_STRING"
                    }
                ],
                "rows": [
                    [
                        "8",
                        process.env.MYSQL_SERVER_MOCK_HOST_NAME
                    ]
                ]
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
            "stmt.regex": "^UPDATE mysql_innodb_cluster_metadata.routers SET attributes =    JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(attributes,    'RWEndpoint', '6446'\\),    'ROEndpoint', '6447'\\),    'RWXEndpoint', '64460'\\),    'ROXEndpoint', '64470'\\) WHERE router_id = .*",
            "exec_time": 0.319936,
            "ok": {}
        },
        {
            "stmt": "COMMIT",
            "exec_time": 0.106985,
            "ok": {}
        }
    ]
}
