var common_stmts = require("common_statements");

if(mysqld.global.host_pattern_id === undefined){
    mysqld.global.host_pattern_id = 0;
}
// we expect create and grant statements for users with those host patterns in that particular
//order
var host_patterns = ["'%'", "'host1'", "'host3%'"];

({
  stmts: function (stmt) {
    var res;

    var options = {
      innodb_cluster_name: "mycluster",
      innodb_cluster_instances: [ ["localhost", 5500], ["localhost", 5510], ["localhost", 5520] ],
      //  /2 because there are 2 grant on PFS statements and we increment host_pattern_id on each
      user_host_pattern: host_patterns[Math.trunc(mysqld.global.host_pattern_id/2)],
    };

    var common_responses = common_stmts.prepare_statement_responses([
      "router_select_schema_version",
      "router_select_group_membership_with_primary_mode",
      "router_select_group_replication_primary_member",
      "router_select_metadata",
      "router_count_clusters_and_replicasets",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances",
      "router_start_transaction",
      "router_commit",
    ], options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex([
      "router_select_hosts",
      "router_insert_into_hosts",
      "router_insert_into_routers",
      "router_delete_old_accounts",
      "router_create_user",
      "router_grant_on_metadata_db",
      "router_update_routers_in_metadata",
    ], options);

    var router_grant_on_pfs_db = common_stmts.prepare_statement_responses_regex([
      "router_grant_on_pfs_db",
    ], options);

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, router_grant_on_pfs_db)) !== undefined) {
      mysqld.global.host_pattern_id++;
      return res;
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
