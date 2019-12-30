/**
 * run 1 node on the current host
 *
 * - 1 PRIMARY
 *
 * via HTTP interface
 *
 * - md_query_count
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if(mysqld.global.gr_id === undefined){
    mysqld.global.gr_id = "00-000";
}

if(mysqld.global.gr_nodes === undefined){
    mysqld.global.gr_nodes = [];
}

if(mysqld.global.notices === undefined){
    mysqld.global.notices = [];
}

if(mysqld.global.md_query_count === undefined){
    mysqld.global.md_query_count = 0;
}

if(mysqld.global.primary_id === undefined){
    mysqld.global.primary_id = 0;
}

var nodes = function(host, port_and_state) {
  return port_and_state.map(function (current_value) {
    return [ current_value[0], host, current_value[0], current_value[1], current_value[2]];
  });
};

({
  stmts: function (stmt) {
    var group_replication_membership_online =
      nodes(gr_node_host, mysqld.global.gr_nodes);

    var options = {
      group_replication_membership: group_replication_membership_online,
      gr_id: mysqld.global.gr_id
    };

    // first node is PRIMARY
    options.group_replication_primary_member = options.group_replication_membership[mysqld.global.primary_id][0];

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses([
      "select_port",
      "router_select_schema_version",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
    ], options);

    var router_select_metadata =
        common_stmts.get("router_select_metadata", options);

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    }
    else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    }
    else if (stmt === "enable_notices" || stmt === "set @@mysqlx_wait_timeout = 28800") {
      return {
        ok: {}
      }
    }
    else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
      return mysqld.global.notices;
  })()
})
