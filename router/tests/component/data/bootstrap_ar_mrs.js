var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var cluster_nodes = gr_memberships.cluster_nodes(
    mysqld.global.gr_node_host, mysqld.global.cluster_nodes)

var options = {
  cluster_type: "ar",
  innodb_cluster_name: "mycluster",
  router_version: mysqld.global.router_version,
  innodb_cluster_instances: cluster_nodes,
  node_pos: mysqld.global.gr_pos,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_view_id_bootstrap_ar",
      "router_select_cluster_id_v2_ar",
      "router_count_clusters_v2_ar",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
    ],
    options);

({
  stmts: function(stmt) {
    var is_primary = options.node_pos === 0;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt == "SELECT GET_LOCK('MRS_METADATA_LOCK', 1)") {
      return {
        result: {
          columns: [{name: "GET_LOCK('MRS_METADATA_LOCK', 1)", type: "LONG"}],
          rows: [["1"]]
        }
      }
    } else if (stmt == "SELECT RELEASE_LOCK('MRS_METADATA_LOCK')") {
      return {
        result: {
          columns:
              [
                {name: "SELECT RELEASE_LOCK('MRS_METADATA_LOCK')", type: "LONG"}
              ],
          rows: [["1"]]
        }
      }
    } else if (
        stmt ==
        "SELECT `major`,`minor`,`patch` FROM mysql_rest_service_metadata.schema_version;") {
      return {
        error: {
          code: 1049,
          sql_state: "42000",
          message: "Unknown database 'mysql_rest_service_metadata'"
        }
      }
    } else if (stmt.includes("SET @OLD_")) {
      return {"ok": {}};
    } else if (stmt.includes("DROP SCHEMA IF EXISTS")) {
      if (is_primary) {
        return {"ok": {}};
      } else {
        return {
          error: {
            code: 1290,
            sql_state: "HY001",
            message:
                "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
          }
        }
      }
    } else {
      return {"ok": {}};
    }
  }
})
