var common_stmts = require("common_statements");

var options = {
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_start_transaction",
      "router_select_view_id_bootstrap_ar",
      "router_commit",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_create_user_if_not_exists",
      "router_select_router_address",
      "router_create_user",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
      "router_drop_users",
    ],
    options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
