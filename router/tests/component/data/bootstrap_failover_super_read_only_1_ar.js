var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");


var gr_members = gr_memberships.members(mysqld.global.gr_members);

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "CLUSTER-ID";
}

var options = {
  cluster_id: mysqld.global.gr_id,
  cluster_type: "ar",
  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: gr_members,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_replication_group_name",
      "router_select_view_id_bootstrap_ar",
      "router_select_cluster_id_v2_ar",
      "router_select_metadata_v2",
      "router_count_clusters_v2_ar",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_ar",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_delete_old_accounts",
      "router_create_user",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
    ],
    options);

var router_insert_into_routers =
    common_stmts.get("router_insert_into_routers", options);

({
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_insert_into_routers.stmt_regex)) {
      return {
        error: {
          code: 1290,
          sql_state: "HY001",
          message:
              "The MySQL server is running with the --super-read-only option so it cannot execute this statement"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
