var common_stmts = require("common_statements");

var options = {
  innodb_cluster_name: "mycluster",
  innodb_cluster_instances: [ ["localhost", 5500], ["localhost", 5510], ["localhost", 5520] ],
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
  "router_rollback",
], options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex([
  "router_select_hosts",
  "router_insert_into_hosts",
  "router_insert_into_routers",
  "router_delete_old_accounts",
], options);

var create_user_long_name_stmt_regex = "^CREATE USER 'mysql_router.*'@'veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost'.*";

({
  stmts: function (stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if ((res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !== undefined) {
      return res;
    }
    else if (stmt.match(create_user_long_name_stmt_regex)) {
      return {
        error: {
          code: 1470,
          sql_state: "HY000",
          message: "String 'veryveryveryveryveryveryveryveryveryveryveryveryveryveryverylonghost' is too long for host name (should be no longer than 60)",
        }
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
