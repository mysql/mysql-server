var common_stmts = require("common_statements");

if (mysqld.global.upd_attr_config_json === undefined) {
  mysqld.global.upd_attr_config_json = "";
}

if (mysqld.global.upd_attr_config_defaults_and_schema_json === undefined) {
  mysqld.global.upd_attr_config_defaults_and_schema_json = "";
}

if (mysqld.global.config_defaults_stored_is_null === undefined) {
  mysqld.global.config_defaults_stored_is_null = 0;
}

var options = {
  cluster_type: "ar",
  innodb_cluster_name: "mycluster",
  router_version: mysqld.global.router_version,
  config_defaults_stored_is_null: mysqld.global.config_defaults_stored_is_null,
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
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_ar",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
      "router_select_metadata_v2_ar_account_verification",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_delete_old_accounts",
      "router_create_user_if_not_exists",
      "router_check_auth_plugin",
      "router_grant_on_metadata_db",
      "router_grant_on_pfs_db",
      "router_grant_on_routers",
      "router_grant_on_v2_routers",
      "router_update_router_options_in_metadata",
      "router_select_config_defaults_stored_ar_cluster",
    ],
    options);

var router_update_attributes =
    common_stmts.get("router_update_routers_in_metadata", options);

var router_store_config_defaults_ar_cluster =
    common_stmts.get("router_store_config_defaults_ar_cluster", options);

({
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (res = stmt.match(router_update_attributes.stmt_regex)) {
      mysqld.global.upd_attr_config_json = res[1];
      return router_update_attributes;
    } else if (
        res = stmt.match(router_store_config_defaults_ar_cluster.stmt_regex)) {
      mysqld.global.upd_attr_config_defaults_and_schema_json = res[1];
      return router_store_config_defaults_ar_cluster;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
