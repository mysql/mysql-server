var common_stmts = require("common_statements");

({
  stmts: function(stmt) {
    var options = {
      cluster_type: "gr",
      gr_id: mysqld.global.gr_id,
      innodb_cluster_name: "test",
      router_version: mysqld.global.router_version,
    };

    var common_responses = common_stmts.prepare_statement_responses(
        [
          "router_set_session_options",
          "router_set_gr_consistency_level",
          "router_select_schema_version",
          "router_select_cluster_type_v2",
          "router_select_current_instance_attributes",
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

          // account verification
          "router_select_metadata_v2_gr",
          "router_select_group_membership",
        ],
        options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex(
        [
          "router_insert_into_routers",
          "router_create_user_if_not_exists",
          "router_check_auth_plugin",
          "router_grant_on_metadata_db",
          "router_grant_on_pfs_db",
          "router_grant_on_routers",
          "router_update_routers_in_metadata",
          "router_update_router_options_in_metadata",
          "router_select_config_defaults_stored_gr_cluster",
        ],
        options);

    var router_grant_on_v2_routers =
        common_stmts.prepare_statement_responses_regex(
            [
              "router_grant_on_v2_routers",
            ],
            options);

    var cu_regex = "CREATE USER IF NOT EXISTS " +
        "'mysql_router1_.*'@'.*' IDENTIFIED WITH `caching_sha2_password` BY '.*'," +
        "'mysql_router1_.*'@'.*' IDENTIFIED WITH `caching_sha2_password` BY '.*'," +
        "'mysql_router1_.*'@'.*' IDENTIFIED WITH `caching_sha2_password` BY '.*'";

    if (stmt.match(cu_regex)) {
      return {"ok": {}};
    }
    var res;

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (
        (res = common_stmts.handle_regex_stmt(
             stmt, router_grant_on_v2_routers)) !== undefined) {
      mysqld.global.host_pattern_id++;
      return res;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
