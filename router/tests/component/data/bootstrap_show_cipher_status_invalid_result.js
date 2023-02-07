var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");


var gr_members = gr_memberships.members(mysqld.global.gr_members);

var options = {
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
  innodb_cluster_name: mysqld.global.cluster_name,
  replication_group_members: gr_members,
  innodb_cluster_instances: gr_members,
  innodb_cluster_hosts: [[8, "dont.query.dns", null]],
  innodb_cluster_user_hosts: [["foo"], ["bar"], ["baz"]],
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_clusterset_present",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
    ],
    options);

var router_show_cipher_status = common_stmts.prepare_statement_responses(
    ["router_show_cipher_status"], options);


({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (stmt.match(router_show_cipher_status.stmt)) {
      return {
        // return one column when 2 are expected
        result: {
          columns:
              [
                {name: "name", type: "STRING"},
              ],
          rows:
              [
                ["foo"],
              ]
        }
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
