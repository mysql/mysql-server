var common_stmts = require("common_statements");

var select_port = common_stmts.get("select_port");
var router_show_cipher_status = common_stmts.get("router_show_cipher_status");

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

({
  stmts: function(stmt) {
    if (stmt == router_show_cipher_status.stmt) {
      return {
        "result": {
          "columns": [
            {"type": "STRING", "name": "Variable_name"},
            {"type": "STRING", "name": "Value"}
          ],
          "rows": [["Ssl_cipher", mysqld.session.ssl_cipher]]
        }
      };
    } else if (stmt === select_port.stmt) {
      increment_event(statement_sql_select);

      return select_port;
    } else if (
        stmt ===
        "SELECT 'collation_connection', @@SESSION.`collation_connection` UNION SELECT 'character_set_client', @@SESSION.`character_set_client` UNION SELECT 'sql_mode', @@SESSION.`sql_mode`") {
      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "collation_connection", type: "STRING"},
            {name: "@@SESSION.collation_connection", type: "STRING"},
          ],
          rows: [
            ["collation_connection", "utf8mb4_0900_ai_ci"],
            ["character_set_client", "utf8mb4"],
            ["sql_mode", "bar"],
          ]
        }
      };
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

      // the trackers that are needed for connection-sharing.
      return {
        ok: {
          session_trackers: [
            {
              type: "system_variable",
              name: "session_track_system_variables",
              value: "*"
            },
            {
              type: "system_variable",
              name: "session_track_gtids",
              value: "OWN_GTID"
            },
            {
              type: "system_variable",
              name: "session_track_transaction_info",
              value: "CHARACTERISTICS"
            },
            {
              type: "system_variable",
              name: "session_track_state_change",
              value: "ON"
            },
            {
              type: "system_variable",
              name: "session_track_state_change",
              value: "ON"
            },
            {
              type: "trx_characteristics",
              value: "",
            },
          ]
        }
      };
    } else if (stmt === "SET @block_me = 1") {
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
