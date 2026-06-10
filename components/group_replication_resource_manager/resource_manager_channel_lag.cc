/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#include <algorithm>

#include "resource_manager_channel_lag.h"
#include "resource_manager_query.h"

const std::string channel_lag_query_title = R"(CHANNEL_LAG_QUERY)";
/*
-- 'group_replication_applier_channel_lag'
-- Returns the time in milliseconds that the transaction that is currently being
applied took between being committed on the source and applied on the target;
-- Returns 0 if there are no transactions in the queue to be applied;
-- Returns NULL if the receiver thread or the applier thread is not running;
-- Returns empty if channel does not exist.

-- 'group_replication_recovery_channel_lag'
-- Returns the time in milliseconds that the transaction that is currently being
applied took between being committed on the source and applied on the target;
-- Returns 0 if there are no transactions in the queue to be applied;
-- Returns NULL if the receiver thread or the applier thread is not running;
-- Returns empty if channel does not exist.
*/
const std::string channel_lag_query = R"(
SELECT ROUND(UNIX_TIMESTAMP(NOW(3)) * 1000) AS timestamp,
    IF(channel_name = 'group_replication_applier' OR channel_name = 'group_replication_recovery',
        CONCAT(channel_name, '_channel_lag'),
        'channel_lag'
    ) AS metric,
    channel_name,
    IF(a.SERVICE_STATE = 'ON',
        -- Check if all transactions received have been applied and that the received transactions are not empty
        IF(
        (SELECT RECEIVED_TRANSACTION_SET FROM performance_schema.replication_connection_status WHERE channel_name = a.channel_name) != ''
        AND GTID_SUBSET(
                (SELECT RECEIVED_TRANSACTION_SET FROM performance_schema.replication_connection_status WHERE channel_name = a.channel_name),
                @@gtid_executed
            ),
            -- Check for the Receiver state if its ON return 0; Otherwise return null
            IF((SELECT SERVICE_STATE
                FROM performance_schema.replication_connection_status
                WHERE channel_name = a.channel_name) = 'ON'
                , 0, NULL),
            -- If we are behind in transactions applied calculate the lag as follows:
            -- Channel_Lag = current_timestamp - <the timestamp of the more recent transaction being applied by any worker,
            -- processed by the coordinator or queued by the receiver>
            ROUND(
                    UNIX_TIMESTAMP(NOW(6)) -
                    NULLIF(
                        UNIX_TIMESTAMP(
                            LEAST(
                                (SELECT IFNULL(NULLIF(GREATEST(
                                            MIN(NULLIF(APPLYING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP, 0)),
                                            MIN(NULLIF(APPLYING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP, 0))),
                                        0),
                                    '2038-01-19 03:14:07.999999')
                                FROM performance_schema.replication_applier_status_by_worker
                                WHERE channel_name = a.channel_name),
                                (SELECT IFNULL(NULLIF(GREATEST(
                                            PROCESSING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP,
                                            PROCESSING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP),
                                        0),
                                    '2038-01-19 03:14:07.999999')
                                FROM performance_schema.replication_applier_status_by_coordinator
                                WHERE channel_name = a.channel_name),
                                (SELECT IFNULL(NULLIF(GREATEST(
                                            QUEUEING_TRANSACTION_IMMEDIATE_COMMIT_TIMESTAMP,
                                            QUEUEING_TRANSACTION_ORIGINAL_COMMIT_TIMESTAMP),
                                        0),
                                    '2038-01-19 03:14:07.999999')
                                FROM performance_schema.replication_connection_status
                                WHERE channel_name = a.channel_name)
                            )
                        ),
                        UNIX_TIMESTAMP('2038-01-19 03:14:07.999999')
                    )
            )
        ),
     NULL) as value
FROM performance_schema.replication_applier_status_by_coordinator AS a
GROUP BY channel_name;
)";

namespace gr_resource_manager {
bool is_numeric(const std::string &str) {
  return std::all_of(str.begin(), str.end(), ::isdigit);
}

#if !defined(NDEBUG)
bool GR_resource_manager::is_debug_variable_present(
    const std::string &variable) {
  gr_resource_manager::Result_Set res;
  char queryBuffer[512];
  std::snprintf(queryBuffer, sizeof(queryBuffer),
                "SELECT @@GLOBAL.DEBUG AS debug_present;");
  static const gr_resource_manager::Query check_variable_presence("debug",
                                                                  queryBuffer);
  gr_resource_manager::Query_Manager::run_query(check_variable_presence, res);

  if (res.size() == 3 && res[2][0].find(variable) != std::string::npos) {
    return true;
  }
  return false;
}

bool insert_values_for_debug(std::vector<lag_record> &records) {
  if (GR_resource_manager::is_debug_variable_present(
          "GR_RM_applier_force_lag")) {
    lag_record record;
    record.timestamp = 1725351747576;
    record.metric = "group_replication_applier_channel_lag";
    record.channel_name = "group_replication_applier";
    record.lag_in_seconds = 1000;
    records.push_back(record);

    record.timestamp = 1725351747576;
    record.metric = "group_replication_recovery_channel_lag";
    record.channel_name = "group_replication_recovery";
    record.lag_in_seconds = 0;
    records.push_back(record);
    return true;
  }
  if (GR_resource_manager::is_debug_variable_present(
          "GR_RM_recovery_force_lag")) {
    lag_record record;
    record.timestamp = 1725351747576;
    record.metric = "group_replication_recovery_channel_lag";
    record.channel_name = "group_replication_recovery";
    record.lag_in_seconds = 1000;
    records.push_back(record);

    record.timestamp = 1725351747576;
    record.metric = "group_replication_applier_channel_lag";
    record.channel_name = "group_replication_applier";
    record.lag_in_seconds = 0;
    records.push_back(record);
    return true;
  }
  return false;
}
#endif

int GR_resource_manager::fetch_channel_lag(std::vector<lag_record> &records) {
#if !defined(NDEBUG)
  if (insert_values_for_debug(records)) return 0;
#endif
  static const gr_resource_manager::Query channel_lag(channel_lag_query_title,
                                                      channel_lag_query);
  gr_resource_manager::Result_Set res;
  if (gr_resource_manager::Query_Manager::run_query(channel_lag, res)) {
    return 1;
  }

  for (const auto &lag_information : res) {
    /* Check if 4 columns are present */
    if (lag_information.size() == 4) {
      // Fetch the timestamp
      const std::string &timestampStr = lag_information[0];
      // Fetch the metric name from the result set.
      const std::string &str1 = lag_information[1];
      // Fetch the channel name from the result set.
      const std::string &str2 = lag_information[2];
      // Fetch the lag from the result set.
      const std::string &valueStr = lag_information[3];

      if (is_numeric(timestampStr) && !str1.empty() && !str2.empty() &&
          is_numeric(valueStr)) {
        lag_record record;
        record.timestamp = std::stoll(timestampStr);
        record.metric = str1;
        record.channel_name = str2;
        record.lag_in_seconds = std::stoi(valueStr);
        records.push_back(record);
      }
    }
  }
  return 0;
}
}  // namespace gr_resource_manager
