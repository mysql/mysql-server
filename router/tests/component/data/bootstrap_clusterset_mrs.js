var common_stmts = require("common_statements");

var options = {
  cluster_type: "gr",

  metadata_schema_version: [2, 1, 0],
  clusterset_present: 1,
  clusterset_target_cluster_id: mysqld.global.target_cluster_id,
  clusterset_data: mysqld.global.clusterset_data,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_start_transaction",
      "router_commit",
      "router_rollback",
      // clusterset specific
      "router_clusterset_cluster_info_current_cluster",
      "router_clusterset_all_nodes",
      "router_clusterset_present",
      "router_clusterset_id_current",
      "router_clusterset_view_id",
    ],
    options);

({
  stmts: function(stmt) {
    var is_primary = options.clusterset_data
                         .clusters[options.clusterset_data.this_cluster_id]
                         .role === "PRIMARY" &&
        options.clusterset_data.this_node_id == 0;
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
