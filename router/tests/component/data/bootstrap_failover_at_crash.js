var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_members = gr_memberships.members(mysqld.global.gr_members);

var options = {
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_members,
  innodb_cluster_instances: gr_members,
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_start_transaction",
      "router_select_replication_group_name",
      "router_select_cluster_id_v2_ar",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_select_router_address",
      "router_delete_old_accounts",
    ],
    options);

var router_create_user_if_not_exists =
    common_stmts.get("router_create_user_if_not_exists", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_create_user_if_not_exists.stmt_regex)) {
      return {
        error: {
          code: 2013,
          sql_state: "HY001",
          message: "Lost connection to MySQL server during query"
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
