var common_stmts = require("common_statements");

var select_port = common_stmts.get("select_port");

var events = {};

var statement_sql_set_option = "statement/sql/set_option";
var statement_sql_select = "statement/sql/select";
var statement_unknown_command = "command/unknown";

// increment the event counter.
//
// if it doesn't exist yet, pretend it is zero
function increment_event(event_name) {
  events[event_name] = (events[event_name] || 0) + 1;
}

// known capabilities
var caps = {
  long_password: 1 << 0,
  found_rows: 1 << 1,
  long_flag: 1 << 2,
  connect_with_schema: 1 << 3,
  no_schema: 1 << 4,
  compress: 1 << 5,
  odbc: 1 << 6,
  local_files: 1 << 7,
  ignore_space: 1 << 8,
  protocol_41: 1 << 9,
  interactive: 1 << 10,
  ssl: 1 << 11,
  transactions: 1 << 13,
  secure_connection: 1 << 15,
  multi_statements: 1 << 16,
  multi_results: 1 << 17,
  ps_multi_results: 1 << 18,
  plugin_auth: 1 << 19,
  connect_attributes: 1 << 20,
  client_auth_with_method_data_varint: 1 << 21,
  expired_passwords: 1 << 22,
  session_track: 1 << 23,
  text_result_with_session_tracking: 1 << 24,
  option_resultset_metadata: 1 << 25,
  query_attributes: 1 << 26,
}

// capabilities per server-version
var caps_3_21 = caps.long_password | caps.found_rows | caps.long_flag |
    caps.connect_with_schema | caps.no_schema;
var caps_3_22 = caps_3_21 | caps.compress | caps.odbc | caps.local_files |
    caps.ignore_space | caps.interactive;
var caps_3_23 = caps_3_22 | caps.ssl | caps.transactions;
var caps_4_0 = caps_3_23;
var caps_4_1 = caps_4_0 | caps.protocol_41 | caps.secure_connection |
    caps.multi_statement | caps.multi_results;
var caps_5_0 = caps_4_1;
var caps_5_1 = caps_5_0;
var caps_5_5 = caps_5_1 | caps.ps_multi_results | caps.plugin_auth;
var caps_5_6 = caps_5_5 | caps.connect_attributes |
    caps.client_auth_method_data_varint | caps.expired_passwords;
var caps_5_7 =
    caps_5_6 | caps.session_track | caps.text_result_with_session_tracking;
var caps_8_0 = caps_5_7 | caps.optional_resultset_metadata |
    caps.compress_zstd | caps.query_attributes;

({
  handshake: {
    greeting: {
      // pretend to be a server that doesn't support session-trackers.
      capabilities: caps_5_6 & ~(caps.ssl | caps.compress | caps.compress_zstd),
      auth_method: "caching_sha2_password",
    },
  },
  stmts: function(stmt) {
    if (stmt === select_port.stmt) {
      increment_event(statement_sql_select);

      return select_port;
    } else if (
        stmt ===
        "SELECT ATTR_NAME, ATTR_VALUE FROM performance_schema.session_account_connect_attrs WHERE PROCESSLIST_ID = CONNECTION_ID() ORDER BY ATTR_NAME") {
      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "ATTR_NAME", type: "STRING"},
            {name: "ATTR_VALUE", type: "STRING"},
          ],
          rows: [
            ["foo", "bar"],
          ]
        }
      };
    } else if (
        stmt ===
        "SELECT EVENT_NAME, COUNT_STAR FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID) WHERE t.PROCESSLIST_ID = CONNECTION_ID() AND COUNT_STAR > 0 ORDER BY EVENT_NAME") {
      var rows = Object.keys(events)
                     .filter(function(key) {
                       // COUNT_START > 0
                       return events[key] > 0;
                     })
                     .sort()  // ORDER BY event_name
                     .reduce(function(collector, key) {
                       var value = events[key];

                       collector.push([key, value]);

                       return collector;
                     }, []);

      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "EVENT_NAME", type: "STRING"},
            {name: "COUNT_START", type: "LONG"},
          ],
          rows: rows,
        }
      };
    } else if (
        stmt.indexOf('SET @@SESSION.session_track_system_variables = "*"') !==
        -1) {
      increment_event(statement_sql_set_option);

      return {ok: {}};
    } else {
      increment_event(statement_unknown_command);

      console.log(stmt);

      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      };
    }
  }
})
