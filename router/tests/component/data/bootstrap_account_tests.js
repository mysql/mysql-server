// debug logging stuff
console.log("----- JS INIT ZERO -----\n\n");
if (mysqld.global.conn_nr === undefined) {
  console.log("conn_nr undefined, setting to 0");
  mysqld.global.conn_nr = 0;  // 0th connection is by wait_for_port_ready()
} else {
  console.log("conn_nr = %d, incrementing by 1", mysqld.global.conn_nr);
  mysqld.global.conn_nr++;
}
var local_conn_nr = mysqld.global.conn_nr;
function inst() {
  return "[JS/MockServer conn#" + local_conn_nr + "] ";
}
console.log(inst() + "----- JS INIT START -----\n\n");

var common_stmts = require("common_statements");

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
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
      "router_select_replication_group_name",  // account verification also
                                               // needs it
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_select_cluster_instance_addresses_v2",
      "router_start_transaction",
      "router_commit",
      "router_clusterset_present",

      // account verification
      "router_select_metadata_v2_gr_account_verification",
      "router_select_group_membership",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers",
      "router_create_user_if_not_exists",  // \
      "router_grant_on_metadata_db",       //  \
      "router_grant_on_pfs_db",            //   > overwritten by most tests
      "router_grant_on_routers",           //  /
      "router_grant_on_v2_routers",        // /
      "router_check_auth_plugin",
      "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata",
      "router_select_router_id",
      "router_select_config_defaults_stored_gr_cluster",
    ],
    options);

// Init stuff.  If blocks are necessary, because account verification during
// bootstrap will open a second connection, which will run this code all over
// again, leading to reset of variables.
if (mysqld.global.custom_responses === undefined)
  mysqld.global.custom_responses = {};
if (mysqld.global.sql_log === undefined) mysqld.global.sql_log = {};
if (mysqld.global.username === undefined) mysqld.global.username = "root";

console.log(inst() + "----- JS INIT END -----\n\n");

({
  handshake: {
    auth: {
      username: mysqld.global.username,
    }
  },
  stmts: function(stmt) {
    // If custom username is defined, change it for following connections.
    // Why do we do it here rather than the place where we init all other
    // stuff?  Because over there, the code runs during connecting phase, which
    // runs a race against set_globals().
    if (!(mysqld.global.custom_auth === undefined)) {
      mysqld.global.username = mysqld.global.custom_auth.username;
    }

    // log query
    // var tmp is a workaround around a limitation we currently have in our
    // testing framework which makes referencing mysqld.global.sql_log[stmt]
    // directly impossible; this limitation may disappear in the future
    if (mysqld.global.sql_log[stmt] === undefined) {
      var tmp = mysqld.global.sql_log;
      tmp[stmt] = 1;
      mysqld.global.sql_log = tmp;
    } else {
      var tmp = mysqld.global.sql_log;
      tmp[stmt]++;
      mysqld.global.sql_log = tmp;
    }

    // return response
    if (mysqld.global.custom_responses.hasOwnProperty(stmt)) {
      return mysqld.global.custom_responses[stmt];
    } else if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else {
      // Until BUG#30314294 is fixed, enabling this may lead to Server Mock
      // getting blocked on write() to STDOUT, causing test failures.
      // Enable only temporarily when you need to debug.
      if (0) {
        console.log(inst() + "UNKNOWN QUERY:\n%s\n\n", stmt);
        console.log(
            inst() + "CUSTOM QUERIES:\n%s\n\n",
            JSON.stringify(mysqld.global.custom_responses));
        console.log(
            inst() + "COMMON RESPONSES:\n%s\n\n",
            JSON.stringify(common_responses));
        console.log(
            inst() + "COMMON REGEX RESPONSES:\n%s\n\n",
            JSON.stringify(common_responses_regex));
        console.log(
            inst() + "ENTIRE GLOBAL:\n%s\n\n", JSON.stringify(mysqld.global));
      }
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
