({
    "stmts": [
        {
            "stmt": "START TRANSACTION",
            "exec_time": 0.082893,
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

        // create temp account to figure out the secure password
        // - fail this, to trigger failover
        {   // ConfigGenerator::generate_compliant_password()
            "stmt.regex": "^CREATE USER mysql_router8_[0-9a-z]{12}@'%' IDENTIFIED WITH mysql_native_password AS '\\*[0-9A-Z]{40}'",
            "error": {
                "code": 2013,
                "message": "Lost connection to MySQL server during query",
                "sql_state": "HY000"
            }
        }
    ]
})
