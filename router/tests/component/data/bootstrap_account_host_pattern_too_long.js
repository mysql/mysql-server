var common_stmts = require("common_statements");

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_select_metadata_v2",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
      "router_clusterset_present",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
    ],
    options);

var create_user_long_name_stmt_regex =
    "^CREATE USER IF NOT EXISTS 'mysql_router.*'@'veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost'.*";

({
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(create_user_long_name_stmt_regex)) {
      return {
        error: {
          code: 1470,
          sql_state: "HY000",
          message:
              "String 'veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost' is too long for host name (should be no longer than 60)",
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
