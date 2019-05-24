/**
 * run 3 nodes on the current host
 *
 * - 1 PRIMARY
 * - 2 SECONDARY
 */

var common_stmts = require("common_statements");

var gr_node_host = "127.0.0.1";

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
       group_replication_membership:  group_replication_membership_online
     }
     // first node is PRIMARY
     options.group_replication_primary_member = options.group_replication_membership[0][0];

     var common_responses = common_stmts.prepare_statement_responses([
      "select_port",
      "router_select_schema_version",
      "router_select_metadata",
      "router_select_group_replication_primary_member",
      "router_select_group_membership_with_primary_mode",
     ], options);

     if (common_responses.hasOwnProperty(stmt)) {
       return common_responses[stmt];
     } else {
       return common_stmts.unknown_statement_response(stmt);
     }
  }
})
