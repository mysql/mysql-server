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
  group_replication_membership: [],
  port: mysqld.session.port,
  innodb_cluster_name: "test",
  innodb_cluster_replicaset_name: "default"
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
          return currentValue.concat([ options["group_replication_single_primary_mode"] ]);
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
      stmt: "SELECT R.replicaset_name, I.mysql_server_uuid, I.role, I.weight, I.version_token, H.location, I.addresses->>'$.mysqlClassic', I.addresses->>'$.mysqlX' FROM mysql_innodb_cluster_metadata.clusters AS F JOIN mysql_innodb_cluster_metadata.replicasets AS R ON F.cluster_id = R.cluster_id JOIN mysql_innodb_cluster_metadata.instances AS I ON R.replicaset_id = I.replicaset_id JOIN mysql_innodb_cluster_metadata.hosts AS H ON I.host_id = H.host_id WHERE F.cluster_name = '" + options.innodb_cluster_name + "';",
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
        rows: options["group_replication_membership"].map(function(currentValue) {
          return [
            options.innodb_cluster_replicaset_name,
            currentValue[0],
            "HA",
            null,
            null,
            "",
            currentValue[1] + ":" + currentValue[2],
            // we don't actually know the xplugin port yet, but on the other side it is also currently unused.
            currentValue[1] + ":" + (parseInt(currentValue[2]) + 10)
          ]
        }),
      }
    }

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
