var defaults = {
  version_comment: "community",
  account_user: "root",
  metadata_schema_version: [1, 0, 1],
  exec_time: 0.0,
  group_replication_single_primary_mode: 1,
  // array-of-array
  // - server-uuid
  // - hostname
  // - port
  // - state
  // - xport (if available and needed)
  group_replication_membership: [],
  port: mysqld.session.port,
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
          var result = currentValue.slice();
          // if group_replication_membership contains x port we need to remove it
          // as this query does not want it
          if (result.length === 5)
              result.splice(-1,1);
          return result.concat([ options["group_replication_single_primary_mode"] ]);
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
      stmt: "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I"
            + " ON R.replicaset_id = I.replicaset_id WHERE F.cluster_name = '"
            + options.innodb_cluster_name + "'"
            + (options.gr_id === undefined || options.gr_id === "" ? "" : (" AND R.attributes->>'$.group_replication_group_name' = '" + options.gr_id + "'"))
            + ";",
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
              "name": "I.addresses->>'$.mysqlClassic'",
              "type": "LONGBLOB"
          },
          {
              "name": "I.addresses->>'$.mysqlX'",
              "type": "LONGBLOB"
          }
        ],
        rows: options["group_replication_membership"].map(function(currentValue) {
          return [
            options.innodb_cluster_replicaset_name,
            currentValue[0],
            "HA",
            null,
            null,
            currentValue[1] + ":" + currentValue[2],
            currentValue[1] + ":" + currentValue[4]
          ]
        }),
      }
    },
    router_count_clusters_and_replicasets: {
          stmt: "SELECT  ((SELECT count(*) FROM mysql_innodb_cluster_metadata.clusters) <= 1  AND (SELECT count(*) FROM mysql_innodb_cluster_metadata.replicasets) <= 1) as has_one_replicaset, (SELECT attributes->>'$.group_replication_group_name' FROM mysql_innodb_cluster_metadata.replicasets)  = @@group_replication_group_name as replicaset_is_ours",
          result: {
            columns: [
              {
                "type": "LONGLONG",
                "name": "has_one_replicaset"
              },
              {
                "type": "LONGLONG",
                "name": "replicaset_is_ours"
              }
            ],
            rows: [
              [
                1,
                1
              ]
            ]
          }
    },
    router_check_member_state:
    {
      stmt: "SELECT member_state FROM performance_schema.replication_group_members WHERE member_id = @@server_uuid",
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
            "replication-1"
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
    router_select_cluster_instances:
    {
      "stmt": "SELECT F.cluster_name, R.replicaset_name, JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic')) FROM mysql_innodb_cluster_metadata.clusters AS F, mysql_innodb_cluster_metadata.instances AS I, mysql_innodb_cluster_metadata.replicasets AS R WHERE R.replicaset_id = (SELECT replicaset_id FROM mysql_innodb_cluster_metadata.instances WHERE mysql_server_uuid = @@server_uuid)AND I.replicaset_id = R.replicaset_id AND R.cluster_id = F.cluster_id",
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
            "name": "JSON_UNQUOTE(JSON_EXTRACT(I.addresses, '$.mysqlClassic'))"
          }
        ],
        rows: options["innodb_cluster_instances"].map(function(currentValue) {
              return [
                options.innodb_cluster_name,
                options.innodb_cluster_replicaset_name,
                currentValue[0] + ":" + currentValue[1],
              ]
        })
      }
    },
    router_start_transaction:
    {
      "stmt": "START TRANSACTION",
      "ok": {}
    },
    router_select_hosts:
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
          return [
             currentValue[0], currentValue[1], currentValue[2]
          ]
        })
      }
    },
    router_select_hosts_join_routers:
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
    router_insert_into_hosts:
    {
        "stmt_regex": "^INSERT INTO mysql_innodb_cluster_metadata.hosts        \\(host_name, location, attributes\\) VALUES \\('"
                      + options.bootstrap_report_host_pattern + "',.*",
      "ok": {
        "last_insert_id": 1
      }
    },
    router_insert_into_routers:
    {
      "stmt_regex": "^INSERT INTO mysql_innodb_cluster_metadata.routers.*",
      "ok": {
        "last_insert_id": 1
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

    // GRANTs
    router_grant_on_metadata_db:
    {
      "stmt_regex": "^GRANT SELECT ON mysql_innodb_cluster_metadata.*"
                    + options.account_host_pattern,
      "ok": {}
    },
    router_grant_on_pfs_db:
    {
      "stmt_regex": "^GRANT SELECT ON performance_schema.*" // this regex covers 2 separate GRANT statements
                    + options.account_host_pattern,
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

    router_update_routers_in_metadata:
    {
      "stmt_regex": "^UPDATE mysql_innodb_cluster_metadata\\.routers.*",
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
        }
       )
     }
    },
    router_drop_users:
    {
        stmt_regex: "^DROP USER mysql_router.*",
        ok: {}
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
