var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var options = {
  cluster_type: "ar",

  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
  innodb_cluster_user_hosts: [["foo"], ["bar"], ["baz"]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_view_id_bootstrap_ar",
      "router_select_cluster_id_v2_ar",
      "router_count_clusters_v2_ar",
      "router_check_member_state",
      "router_select_members_count",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_ar",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_delete_old_accounts",
    ],
    options);

var router_drop_users = common_stmts.get("router_drop_users", options);

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt.match(router_drop_users.stmt_regex)) {
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
