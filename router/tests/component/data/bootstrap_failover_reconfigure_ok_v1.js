var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");


var gr_members =
  gr_memberships.members(mysqld.global.gr_members);

var options = {
  metadata_schema_version: [1, 0, 2],
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members:  gr_members,

  innodb_cluster_instances: [ ["127.0.0.1", 13001], ["127.0.0.1", 13002], ["127.0.0.1", 13003] ],
  innodb_cluster_hosts: [ [ 8, "dont.query.dns", null ]],
};

var common_responses = common_stmts.prepare_statement_responses([
  "router_start_transaction",
  "router_commit",
], options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex([
  "router_insert_into_hosts_v1",
  "router_insert_into_routers_v1",
  "router_select_hosts_v1",
  "router_select_hosts_join_routers_v1",
  "router_create_user_if_not_exists",
  "router_grant_on_metadata_db",
  "router_grant_on_pfs_db",
  "router_grant_on_routers",
  "router_update_routers_in_metadata_v1",
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
