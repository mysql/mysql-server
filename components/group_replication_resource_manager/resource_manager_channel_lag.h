/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.
*/

#ifndef GR_RESOURCE_MANAGER_CHANNEL_LAG
#define GR_RESOURCE_MANAGER_CHANNEL_LAG

#include <my_inttypes.h>
#include <string>
#include <vector>

namespace gr_resource_manager {
/**
 Stores the channel lag information.
*/
struct lag_record {
  /// Stores the timestamp when information was fetched.
  long long timestamp{0};
  /// Either group_replication_applier_channel_lag or
  /// group_replication_recovery_channel_lag.
  std::string metric{""};
  /// Either group_replication_applier or group_replication_recovery.
  std::string channel_name{""};
  /// Lag in seconds.
  uint lag_in_seconds{0};
};

/**
  @class GR_resource_manager

  This class will be used to execute SQL Queries.
  For each SQL Query run a new function needs to be implemented.
 */
class GR_resource_manager {
 public:
  /**
   Execute a SQL Query and fetch the lag for group replication applier channel
   and recovery channel.

   @param[out] record Group replication channels lag

   @retval 0 Query execution success
   @retval 1 Query execution failed
 */
  static int fetch_channel_lag(std::vector<lag_record> &record);

#if !defined(NDEBUG)
  /**
    Check if the passed string is present in GLOBAL.DEBUG or not.

    @param[in] variable debug string to check in GLOBAL.DEBUG

    @retval true string is present in GLOBAL.DEBUG
    @retval false string is not present in GLOBAL.DEBUG
  */
  static bool is_debug_variable_present(const std::string &variable);
#endif
};
}  // namespace gr_resource_manager

#endif  // GR_RESOURCE_MANAGER_CHANNEL_LAG
