var common_stmts = require("common_statements");

var options = {
    innodb_cluster_hosts: [ [ 8, "dont.query.dns", null ]],
};

var common_responses = common_stmts.prepare_statement_responses([
  "router_start_transaction",
  "router_commit",
], options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex([
  "router_select_hosts_join_routers",
  "router_delete_old_accounts",
  "router_create_user",
  "router_grant_on_metadata_db",
  "router_grant_on_pfs_db",
  "router_update_routers_in_metadata",
  "router_drop_users",
], options);

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
