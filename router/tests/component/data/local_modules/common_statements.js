var defaults = {
  version_comment: "community",
  account_user: "root",
  metadata_schema_version: [2, 0, 0],
  exec_time: 0.0,
  group_replication_single_primary_mode: 1,
  // array-of-array
  // - server-uuid
  // - hostname
  // - port
  // - state
  // - xport (if available and needed)
  group_replication_membership: [],
  group_replication_name: "cluster-specific-id",
  port: mysqld.session.port,
  cluster_id: "cluster-specific-id",
  innodb_cluster_name: "test",
  innodb_cluster_replicaset_name: "default",
  use_bootstrap_big_data: false,
  replication_group_members: [],
  innodb_cluster_instances: [
    ["localhost", "5500"],
    ["localhost", "5510"],
    ["localhost", "5520"],
  ],
  innodb_cluster_hosts: [],
  innodb_cluster_user_hosts: [],
  bootstrap_report_host_pattern: ".*",
  account_host_pattern: ".*",
  account_user_pattern: "mysql_router1_[0-9a-z]{12}",
  account_pass_pattern: "\\*[0-9A-Z]{40}",
  create_user_warning_count: 0,
  create_user_show_warnings_results: [],

  user_host_pattern: ".*",
  cluster_type: "gr",
  view_id: 1,
  primary_port: 0,
  router_id: 1,
  // let the test that uses it set it explicitly, going with some default would mean failures
  // each time the version is bumped up (which we don't even control)
  router_version: "",
  rest_user_credentials: []
};

function ensure_type(options, field, expected_type) {
  var current_type = typeof(options[field]);

  if (expected_type === "array") {
    tested_type = "object";
  } else {
    tested_type = expected_type;
  }

  if (current_type !== tested_type) throw "expected " + field + " to be a " + expected_type + ", got " + current_type;
  if (expected_type === "array") {
    if (!Array.isArray(options[field])) throw "expected " + field + " to be a " + expected_type + ", got " + current_type;
  }
}

/**
 * create response for commonly used statements
 *
 * @param {string} stmt_key statement key
 * @param {object} [options] options replacement values for statement text and response
 * @returns On success, response object, otherwise 'undefined'
 */
exports.get = function get(stmt_key, options) {
  // let 'options' overwrite the 'defaults'
  options = Object.assign({}, defaults, options);

  ensure_type(options, "version_comment", "string");
  ensure_type(options, "account_user", "string");
  ensure_type(options, "metadata_schema_version", "array");
  ensure_type(options, "exec_time", "number");
  ensure_type(options, "port", "number");

  var statements = {
    mysql_client_select_version_comment: {
      stmt: "select @@version_comment limit 1",
      exec_time: options["exec_time"],
      result: {
        columns: [
          {
            name: "@@version_comment",
            type: "STRING"
          }
        ],
        rows: [
          [ options["version_comment"] ]
        ]
      }
    },
    mysql_client_select_user: {
      stmt: "select USER()",
      result: {
        columns: [
          {
            name: "USER()",
            type: "STRING"
          }
        ],
        rows: [
          [ options["account_user"] ]
        ]
      }
    },
    select_port: {
      stmt: "select @@port",
      result: {
        columns: [
          {
            name: "@@port",
            type: "LONG"
          }
        ],
        rows: [
          [ options["port"] ]
        ]
      }
    },
    router_select_schema_version: {
      stmt: "SELECT * FROM mysql_innodb_cluster_metadata.schema_version",
      exec_time: options["exec_time"],
      result: {
        columns: [
          {
            type: "LONGLONG",
            name: "major"
          },
          {
            type: "LONGLONG",
            name: "minor"
          },
          {
            type: "LONGLONG",
            name: "patch"
          }
        ],
        rows: [
          options["metadata_schema_version"]
        ]
      }
    },
    router_select_group_membership_with_primary_mode: {
      stmt: "SELECT member_id, member_host, member_port, member_state, @@group_replication_single_primary_mode FROM performance_schema.replication_group_members WHERE channel_name = 'group_replication_applier'",
      exec_time: options["exec_time"],
      result: {
        columns: [
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
        rows: options["group_replication_membership"].map(function(currentValue) {
          return [currentValue[0], currentValue[1], currentValue[2],
                  currentValue[3], options["group_replication_single_primary_mode"]];
        }),
      }
    },
    router_select_group_replication_primary_member: {
      "stmt": "show status like 'group_replication_primary_member'",
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
              options["group_replication_primary_member"]
          ]
        ]
      }
    },

    router_select_metadata: {
      stmt: "SELECT R.replicaset_name, I.mysql_server_uuid, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I"
            + " ON R.replicaset_id = I.replicaset_id WHERE F.cluster_name = '"
            + options.innodb_cluster_name + "'"
            + (options.gr_id === undefined || options.gr_id === "" ? "" : (" AND R.attributes->>'$.group_replication_group_name' = '" + options.gr_id + "'")),
      result : {
        columns : [
          {
              "name": "replicaset_name",
              "type": "VAR_STRING"
          },
          {
              "name": "mysql_server_uuid",
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
        rows: options["group_replication_membership"].map(function(currentValue) {
          var xport = currentValue[4] === undefined ? 0 : currentValue[4];
          return [
            options.innodb_cluster_replicaset_name,
            currentValue[0],
            currentValue[1] + ":" + currentValue[2],
            currentValue[1] + ":" + xport
          ]
        }),
      }
    },

    router_select_metadata_v2_gr: {
      stmt: "select I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes from mysql_innodb_cluster_metadata.v2_instances I join mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = C.cluster_id where C.cluster_name = '"
            + options.innodb_cluster_name + "'"
            + (options.gr_id === undefined || options.gr_id === "" ? "" : (" AND C.group_name = '" + options.gr_id + "'")),
      result : {
        columns : [
          {
              "name": "mysql_server_uuid",
              "type": "VAR_STRING"
          },
          {
              "name": "I.addresses->>'$.mysqlClassic'",
              "type": "LONGBLOB"
          },
          {
              "name": "I.addresses->>'$.mysqlX'",
              "type": "LONGBLOB"
          },
          {
               "name": "I.attributes",
               "type": "VAR_STRING"
          }
        ],
        rows: options["group_replication_membership"].map(function(currentValue) {
          var xport = currentValue[4] === undefined ? 0 : currentValue[4];
          var attributes = currentValue[5] === undefined ? "" : currentValue[5];
          return [
            currentValue[0],
            currentValue[1] + ":" + currentValue[2],
            currentValue[1] + ":" + xport,
            attributes
          ]
        }),
      }
    },

    router_select_metadata_v2_ar: {
      stmt: "select M.member_id, I.endpoint, I.xendpoint, M.member_role, I.attributes from mysql_innodb_cluster_metadata.v2_ar_members M join mysql_innodb_cluster_metadata.v2_instances I on I.instance_id = M.instance_id join mysql_innodb_cluster_metadata.v2_ar_clusters C on I.cluster_id = C.cluster_id"
            + (options.cluster_id === undefined || options.cluster_id === "" ? "" : (" where C.cluster_id = '" + options.cluster_id + "'")),
      result : {
        columns : [
          {
              "name": "member_id",
              "type": "VAR_STRING"
          },
          {
              "name": "I.endpoint",
              "type": "LONGBLOB"
          },
          {
              "name": "I.xendpoint",
              "type": "LONGBLOB"
          },
          {
              "name": "I.member_role",
              "type": "VAR_STRING"
          },
          {
              "name": "I.attributes",
              "type": "VAR_STRING"
          }
        ],
        rows: options["group_replication_membership"].map(function(currentValue) {
          var xport = currentValue[4] === undefined ? 0 : currentValue[4];
          var attributes = currentValue[5] === undefined ? "" : currentValue[5];
          return [
            currentValue[0],
            currentValue[1] + ":" + currentValue[2],
            currentValue[1] + ":" + xport,
            currentValue[2] === options.primary_port ? "PRIMARY" : "SECONDARY",
            attributes
          ]
        }),
      }
    },

    router_select_view_id_v2_ar: {
        stmt: "select view_id from mysql_innodb_cluster_metadata.v2_ar_members where CAST(member_id AS char ascii) = CAST(@@server_uuid AS char ascii)"
            + (options.cluster_id === undefined || options.cluster_id === "" ? "" : (" and cluster_id = '" + options.cluster_id + "'")),
        result : {
          columns : [,
            {
                "name": "view_id",
                "type": "LONG"
            },
          ],
          rows: [
            [
              options.view_id
            ]
          ]
        }
      },

    router_select_view_id_bootstrap_ar: {
      stmt: "select view_id from mysql_innodb_cluster_metadata.v2_ar_members where CAST(member_id AS char ascii) = CAST(@@server_uuid AS char ascii)",
      result : {
        columns : [,
          {
            "name": "view_id",
            "type": "LONG"
          },
        ],
        rows: [
          [
            options.view_id
          ]
        ]
      }
    },

    router_check_member_state:
    {
      stmt: "SELECT member_state FROM performance_schema.replication_group_members WHERE CAST(member_id AS char ascii) = CAST(@@server_uuid AS char ascii)",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "member_state"
          }
        ],
        rows: [
          [
            "ONLINE"
          ]
        ]
      }
    },

    router_select_members_count:
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

    router_select_replication_group_name:
    {
      "stmt": "select @@group_replication_group_name",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "@@group_replication_group_name"
          }
        ],
        "rows": [
          [
            options.group_replication_name
          ]
        ]
      },
    },

    router_start_transaction:
    {
      "stmt": "START TRANSACTION",
      "ok": {}
    },

    router_select_router_address:
    {
      stmt_regex: "SELECT address FROM mysql_innodb_cluster_metadata.v2_routers WHERE router_id = .*",
      result: {
        columns: [
           {
                "type": "STRING",
                "name": "address"
            }
        ],
        rows: options["innodb_cluster_hosts"].map(function(currentValue) {
          return [
            currentValue[1]
          ]
        })
      }
    },

    router_select_router_id:
    {
      stmt_regex: "SELECT router_id FROM mysql_innodb_cluster_metadata.v2_routers WHERE router_name = .*",
      result: {
        columns: [
           {
                "type": "LONG",
                "name": "router_id"
            }
        ],
        rows: [
            options.router_id
        ]
      }
    },

    router_select_hosts_v1:
    {
      stmt_regex: "^SELECT host_id, host_name, ip_address FROM mysql_innodb_cluster_metadata.hosts WHERE host_name = '"
                  + options.bootstrap_report_host_pattern + "' LIMIT 1",
      result: {
        columns: [
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
        ],
        rows: options["innodb_cluster_hosts"].map(function(currentValue) {
          var xport = currentValue[2] === undefined ? 0 : currentValue[2];
          return [
             currentValue[0], currentValue[1], currentValue[2]
          ]
        })
      }
    },

    router_select_hosts_join_routers_v1:
    {
      stmt_regex: "SELECT h.host_id, h.host_name FROM mysql_innodb_cluster_metadata.routers r JOIN mysql_innodb_cluster_metadata.hosts h    ON r.host_id = h.host_id WHERE r.router_id = .*",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "host_id"
          },
          {
            "type": "STRING",
            "name": "host_name"
          }
        ],
        rows: options["innodb_cluster_hosts"].map(function(currentValue) {
          return [
             currentValue[0], currentValue[1]
          ]
        })
      }
    },

    router_insert_into_hosts_v1:
    {
        "stmt_regex": "^INSERT INTO mysql_innodb_cluster_metadata.hosts        \\(host_name, location, attributes\\) VALUES \\('"
                      + options.bootstrap_report_host_pattern + "',.*",
      "ok": {
        "last_insert_id": 1
      }
    },

    router_insert_into_routers_v1:
    {
      "stmt_regex": "^INSERT INTO mysql_innodb_cluster_metadata.routers.*",
      "ok": {
        "last_insert_id": 1
      }
    },

    router_insert_into_routers:
    {
      "stmt_regex": "^INSERT INTO mysql_innodb_cluster_metadata.v2_routers.*",
      "ok": {
        "last_insert_id": 1
      }
    },

    // delete all old accounts if necessarry (ConfigGenerator::delete_account_for_all_hosts())
    router_delete_old_accounts:
    {
      "stmt_regex": "^SELECT host FROM mysql.user WHERE user = '.*'",
      "result": {
        "columns": [
          {
            "type": "LONGLONG",
            "name": "COUNT..."
          }
        ],
        "rows": options["innodb_cluster_user_hosts"],
      }
    },

    router_create_user:
    {
      "stmt_regex": "^CREATE USER 'mysql_router1_[0-9a-z]{12}'@"
                    + options.user_host_pattern
                    + " IDENTIFIED WITH mysql_native_password AS '\\*[0-9A-Z]{40}'",
      "ok": {}
    },

    router_grant_on_metadata_db:
    {
      "stmt_regex": "^GRANT SELECT, EXECUTE ON mysql_innodb_cluster_metadata.*"
                     + options.user_host_pattern,
      "ok": {}
    },
    router_grant_on_pfs_db:
    {
      "stmt_regex": "^GRANT SELECT ON performance_schema.*"
                    + options.user_host_pattern,
      "ok": {}
    },
    router_grant_on_routers:
    {
      "stmt_regex": "^GRANT INSERT, UPDATE, DELETE ON mysql_innodb_cluster_metadata\\.routers.*"
                    + options.user_host_pattern,
      "ok": {}
    },
    router_grant_on_v2_routers:
    {
      "stmt_regex": "^GRANT INSERT, UPDATE, DELETE ON mysql_innodb_cluster_metadata\\.v2_routers.*"
                    + options.user_host_pattern,
      "ok": {}
    },

    router_update_routers_in_metadata_v1:
    {
      "stmt_regex": "^UPDATE mysql_innodb_cluster_metadata\\.routers SET attributes =    "
            + "JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(IF\\(attributes IS NULL, '\\{\\}', attributes\\),    "
            + "'\\$\\.RWEndpoint', '.*'\\),    '\\$\\.ROEndpoint', '.*'\\),    '\\$\\.RWXEndpoint', '.*'\\),    "
            + "'\\$\\.ROXEndpoint', '.*'\\),    '\\$.MetadataUser', 'mysql_router.*'\\) "
            + "WHERE router_id = .*",
      "ok": {}
    },

    router_update_routers_in_metadata:
    {
      "stmt_regex": "^UPDATE mysql_innodb_cluster_metadata\\.v2_routers SET attributes =    "
            + "JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(JSON_SET\\(IF\\(attributes IS NULL, '\\{\\}', attributes\\),    "
            + "'\\$\\.RWEndpoint', '.*'\\),    '\\$\\.ROEndpoint', '.*'\\),    '\\$\\.RWXEndpoint', '.*'\\),    "
            + "'\\$\\.ROXEndpoint', '.*'\\),    '\\$.MetadataUser', '.*'\\), version = '.*', cluster_id = '.*' "
            + "WHERE router_id = .*",
      "ok": {}
    },

    router_commit:
    {
      stmt: "COMMIT",
      ok: {}
    },

    router_rollback:
    {
      stmt: "ROLLBACK",
      ok: {}
    },

    router_replication_group_members:
    {
      stmt: "SELECT member_host, member_port   FROM performance_schema.replication_group_members  /*!80002 ORDER BY member_role */",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "member_host"
          },
          {
            "type": "LONG",
            "name": "member_port"
          }
        ],
        rows: options["replication_group_members"].map(function(currentValue) {
          return [
            currentValue[0], currentValue[1],
          ]
        }),
      }
    },

    router_drop_users:
    {
      stmt_regex: "^DROP USER IF EXISTS 'mysql_router.*",
      ok: {}
    },

    router_select_metadata_v2: {
      stmt: "select 'default' as replicaset_name, I.mysql_server_uuid, I.endpoint, I.xendpoint, I.attributes from " +
              "mysql_innodb_cluster_metadata.v2_instances I join " +
              "mysql_innodb_cluster_metadata.v2_gr_clusters C on I.cluster_id = " +
              "C.cluster_id where C.cluster_name =  " +
           options.innodb_cluster_name + "'" +
           (options.gr_id === undefined || options.gr_id === "" ? "" : (" AND R.attributes->>'$.group_replication_group_name' = '" + options.gr_id + "'")),
      result : {
        columns : [
          {
            "name": "replicaset_name",
            "type": "VAR_STRING"
          },
          {
            "name": "mysql_server_uuid",
            "type": "VAR_STRING"
          },
          {
            "name": "I.addresses->>'$.mysqlClassic'",
            "type": "LONGBLOB"
          },
          {
            "name": "I.addresses->>'$.mysqlX'",
            "type": "LONGBLOB"
          },
          {
            "name": "I.attributes",
            "type": "VAR_STRING"
          }
        ],
        rows: options["group_replication_membership"].map(function(currentValue) {
            var xport = currentValue[4] === undefined ? 0 : currentValue[4];
            var attributes = currentValue[5] === undefined ? "" : currentValue[5];
              return [
                options.innodb_cluster_replicaset_name,
                currentValue[0],
                currentValue[1] + ":" + currentValue[2],
                currentValue[1] + ":" + xport,
                attributes
              ]
          }),
        }
    },

    router_count_clusters_v1: {
      stmt: "select count(*) from " +
            "mysql_innodb_cluster_metadata.clusters",
      result: {
        columns: [
          {
            "type": "LONGLONG",
            "name": "count(*)"
          }
         ],
         rows: [
           [
             1
           ]
         ]
      }
    },

    router_count_clusters_v2: {
      stmt: "select count(*) from " +
             "mysql_innodb_cluster_metadata.v2_gr_clusters",
      result: {
        columns: [
          {
            "type": "LONGLONG",
            "name": "count(*)"
          }
         ],
         rows: [
           [
             1
           ]
         ]
      }
    },

    router_count_clusters_v2_ar: {
      stmt: "select count(*) from " +
            "mysql_innodb_cluster_metadata.v2_ar_clusters",
      result: {
        columns: [
          {
            "type": "LONGLONG",
            "name": "count(*)"
           }
        ],
        rows: [
          [
            1
          ]
        ]
      }
    },

    router_show_cipher_status:
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

    router_select_cluster_instances_v1:
    {
      "stmt": "SELECT F.cluster_id, F.cluster_name, " +
            "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) " +
            "FROM " +
            "mysql_innodb_cluster_metadata.clusters AS F, " +
            "mysql_innodb_cluster_metadata.instances AS I, " +
            "mysql_innodb_cluster_metadata.replicasets AS R " +
            "WHERE R.replicaset_id = (SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances " +
            "WHERE CAST(mysql_server_uuid AS char ascii) = CAST(@@server_uuid AS char ascii)) AND I.replicaset_id = R.replicaset_id " +
            "AND R.cluster_id = F.cluster_id",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "cluster_id"
          },
          {
            "type": "STRING",
            "name": "cluster_name"
          },
          {
            "type": "STRING",
            "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))"
          }
        ],
        rows: options["innodb_cluster_instances"].map(function(currentValue) {
              return [
                options.cluster_id,
                options.innodb_cluster_name,
                currentValue[0] + ":" + currentValue[1],
              ]
        })
      }
    },

    router_select_cluster_instances_v2:
    {
      "stmt": "select c.cluster_id, c.cluster_name, i.address from " +
          "mysql_innodb_cluster_metadata.v2_instances i join " +
          "mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = " +
          "i.cluster_id",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "cluster_id"
          },
          {
            "type": "STRING",
            "name": "cluster_name"
          },
          {
            "type": "STRING",
            "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))"
          }
        ],
        rows: options["innodb_cluster_instances"].map(function(currentValue) {
              return [
                options.cluster_id,
                options.innodb_cluster_name,
                currentValue[0] + ":" + currentValue[1],
              ]
        })
      }
    },

    router_select_cluster_instance_addresses_v2:
    {
      "stmt": "select i.address from mysql_innodb_cluster_metadata.v2_instances i join mysql_innodb_cluster_metadata.v2_clusters c on c.cluster_id = i.cluster_id",
      result: {
        columns: [
          {
            "type": "STRING",
            "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))"
          }
        ],
        rows: options["innodb_cluster_instances"].map(function(currentValue) {
              return [
                currentValue[0] + ":" + currentValue[1],
              ]
        })
      }
    },

    router_select_cluster_type_v2:
    {
      "stmt": "select cluster_type from mysql_innodb_cluster_metadata.v2_this_instance",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "cluster_type"
          }
         ],
         "rows": options.cluster_type === "" ? [] : [[options.cluster_type]]
      }
    },

    // CREATE USER IF NOT EXISTS is the default way of creating users
    router_create_user_if_not_exists:
    {
      "stmt_regex": "^CREATE USER IF NOT EXISTS '" + options.account_user_pattern + "'@'" + options.account_host_pattern + "' IDENTIFIED WITH mysql_native_password AS '" + options.account_pass_pattern + "'",
      "ok": {
         warning_count : options.create_user_warning_count
       }
    },
    // CREATE USER (without IF NOT EXISTS) is triggered by --account-create always
    router_create_user:
    {
      "stmt_regex": "^CREATE USER '" + options.account_user_pattern + "'@'" + options.account_host_pattern + "' IDENTIFIED WITH mysql_native_password AS '" + options.account_pass_pattern + "'",
      "ok": {}
    },

    router_select_cluster_id_v2_ar:
    {
      "stmt": "select cluster_id from mysql_innodb_cluster_metadata.v2_ar_clusters",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "cluster_id"
          }
         ],
         "rows": [[options.cluster_id]]
      }
    },

    router_update_version_v1:
    {
      "stmt_regex": "UPDATE mysql_innodb_cluster_metadata\.routers" +
              " SET attributes = JSON_SET\(IF\(attributes IS NULL, '\{\}', attributes\), " +
              "'\$\.version', .*\) WHERE router_id = " + options.router_id,
      "ok": {}
    },

    // the exact match, not regex
    router_update_version_strict_v1:
    {
      "stmt": "UPDATE mysql_innodb_cluster_metadata.routers" +
              " SET attributes = JSON_SET(IF(attributes IS NULL, '{}', attributes), " +
              "'$.version', '" + options.router_version + "') WHERE router_id = " + options.router_id,
      "ok": {}
     },

    // this query will only be issued if CREATE USER [IF NOT EXISTS] returned warning_count > 0
    router_create_user_show_warnings: {
      stmt: "SHOW WARNINGS",
      // SHOW WARNINGS example output
      // +-------+------+---------------------------------------------+
      // | Level | Code | Message                                     |
      // +-------+------+---------------------------------------------+
      // | Note  | 3163 | Authorization ID 'bla'@'h1' already exists. |
      // | Note  | 3163 | Authorization ID 'bla'@'h3' already exists. |
      // +-------+------+---------------------------------------------+
      result: {
        columns: [
          {
            type: "STRING",
            name: "Level"
          },
          {
            type: "LONG",
            name: "Code"
          },
          {
            type: "STRING",
            name: "Message"
          }
        ],
        rows: options.create_user_show_warnings_results
      }
    },

    router_update_version_v2:
    {
      "stmt_regex": "UPDATE mysql_innodb_cluster_metadata\.v2_routers set version = .*" +
              " where router_id = .+",
      "ok": {}
    },

    router_update_version_strict_v2:
    {
      "stmt": "UPDATE mysql_innodb_cluster_metadata.v2_routers set version = '" + options.router_version +
              "' where router_id = " + options.router_id,
      "ok": {}
    },

    router_update_last_check_in_v2:
    {
      "stmt": "UPDATE mysql_innodb_cluster_metadata.v2_routers set last_check_in = " +
              "NOW() where router_id = " + options.router_id,
      "ok": {}
    },

    router_set_session_options:
    {
      "stmt": "SET @@SESSION.autocommit=1, @@SESSION.character_set_client=utf8, " +
              "@@SESSION.character_set_results=utf8, @@SESSION.character_set_connection=utf8, " +
              "@@SESSION.sql_mode='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES," +
              "NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION', " +
              "@@SESSION.optimizer_switch='derived_merge=on'",
      "ok": {}
    },

    router_set_gr_consistency_level:
    {
      "stmt": "SET @@SESSION.group_replication_consistency='EVENTUAL'",
      "ok": {}
    },

    router_select_rest_accounts_credentials:
    {
      "stmt": "SELECT user, authentication_string, privileges, " +
              "authentication_method FROM " +
              "mysql_innodb_cluster_metadata.v2_router_rest_accounts WHERE " +
              "cluster_id=(SELECT cluster_id FROM " +
              "mysql_innodb_cluster_metadata.v2_clusters WHERE cluster_name='"
              + options.innodb_cluster_name + "')",
      "result": {
        "columns": [
          {
            "type": "STRING",
            "name": "user"
          },
          {
            "type": "STRING",
            "name": "authentication_string"
          },
          {
            "type": "STRING",
            "name": "privileges"
          },
          {
            "type": "STRING",
            "name": "authentication_method"
          }
        ],
        "rows": options["rest_user_credentials"].map(function(currentValue) {
              return [
                currentValue[0],
                currentValue[1],
                currentValue[2] === "" ? null : currentValue[2],
                currentValue[3],
              ]
        })
      }
    },
  };

  return statements[stmt_key];
}

/**
 * create the response for commonly used statements
 *
 * @param {array} stmt_keys - statement keys
 * @param {array} options   - parameters
 * @returns {object} object of 'statement text': response object
 */
exports.prepare_statement_responses = function(stmt_keys, options) {
  return stmt_keys.reduce(function(acc, stmt_key) {
    // lookup the results by stmt_key
    var res = exports.get(stmt_key, options);
    acc[res.stmt] = res;

    return acc;
  }, {});
}


/**
 * create the response for commonly used regex statements
 *
 * @param {array} stmt_keys - statement keys
 * @param {array} options   - parameters
 * @returns {object} object of 'statement text': response object
 */
exports.prepare_statement_responses_regex = function(stmt_keys, options) {
  return stmt_keys.reduce(function(acc, stmt_key) {
    // lookup the results by stmt_key
    var res = exports.get(stmt_key, options);
    acc[res.stmt_regex] = res;

    return acc;
  }, {});
}

/**
 * create error-response for unknown statements
 *
 * @param {string} stmt statement text
 * @returns error response
 */
exports.unknown_statement_response = function(stmt) {
  return {
    error: {
      code: 1273,
      sql_state: "HY001",
      message: "Syntax Error at: " + stmt
    }
  }
}

/**
 * checks if a given statment matches any stmt_regex in
 * in the responses object
 *
 * @param {string} regex_stmt regex statement text
 * @param {object} common_responses_regex object containing common responses
 * @returns  response if any matches the statmenet or undefined
 */
exports.handle_regex_stmt = function(regex_stmt, common_responses_regex) {
  for (var stmt in common_responses_regex) {
    if (regex_stmt.match(stmt))
      return common_responses_regex[stmt];
  }

  return undefined;
};
