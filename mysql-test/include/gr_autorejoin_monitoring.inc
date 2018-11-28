# ==== Purpose ====
#
# Enables thread stage events for the auto-rejoin process.
# Adds the following SQL functions to easily monitor auto-rejoins in GR:
#
#   IS_AUTOREJOIN_RUNNING()
#     + Returns a flag indicating whether or not an auto-rejoin process is
#       currently running.
#
#   GET_NUMBER_RETRIES()
#     + Returns the number of retries that have been attempted in a running
#       auto-rejoin process.
#
#   GET_TIME_UNTIL_NEXT_RETRY()
#     + Returns the time remaining until the next retry in seconds, for a
#       running auto-rejoin process. If no auto-rejoin process is running, it
#       returns -1.
#
#   GET_LAST_AUTOREJOIN()
#     + Returns the UNIX timestamp of the last auto-rejoin process that was
#       run. If no auto-rejoin process has been attempted yet, returns -1.
#
#   GET_COUNT_AUTOREJOIN()
#     + Returns the number of times the auto-rejoin process has been run.
#
# ==== Usage ====
#
# --source include/gr_autorejoin_monitoring.inc
#
# NOTE: The functions created in this include can be dropped by sourcing the
# include/gr_end_autorejoin_monitoring.inc file.
#
--let $include_filename = gr_autorejoin_monitoring.inc
--source include/begin_include_file.inc

--disable_query_log
SET SESSION sql_log_bin = 0;

DELIMITER //;
CREATE FUNCTION IS_AUTOREJOIN_RUNNING() RETURNS BOOLEAN
BEGIN
  DECLARE autorejoin_running INTEGER;
  DECLARE ret BOOL;
  SET autorejoin_running = -1;
  SET ret = FALSE;

  -- There is only one auto-rejoin thread. If there is a thread stage event
  -- with the auto-rejoin name, then an auto-rejoin process is being run.
  SELECT COUNT(*) FROM performance_schema.events_stages_current
    WHERE EVENT_NAME LIKE '%auto-rejoin%'
    INTO autorejoin_running;

  IF autorejoin_running = 1 THEN
    SET ret = TRUE;
  END IF;

  RETURN ret;
END //

CREATE FUNCTION GET_NUMBER_RETRIES()
  RETURNS INT
BEGIN
  DECLARE ret INT;
  DECLARE autorejoin_running BOOL;
  SET ret = -1;
  SET autorejoin_running = FALSE;

  SELECT IS_AUTOREJOIN_RUNNING() INTO autorejoin_running;

  -- The WORK_COMPLETED field of the events_stage_current performance schema
  -- table contains the number of attempts within a running auto-rejoin process.
  IF autorejoin_running = TRUE THEN
    SELECT WORK_COMPLETED FROM performance_schema.events_stages_current
      WHERE EVENT_NAME LIKE '%auto-rejoin%'
      INTO ret;
  END IF;

  RETURN ret;
END //

CREATE FUNCTION GET_TIME_UNTIL_NEXT_RETRY(wait_interval INTEGER)
  RETURNS DOUBLE
BEGIN
  DECLARE ret DOUBLE;
  DECLARE autorejoin_running BOOL;
  DECLARE time_in_secs DOUBLE;
  DECLARE num_retries INT;
  DECLARE sleep_interval INT;
  SET ret = -1;
  SET autorejoin_running = FALSE;
  SET time_in_secs = 0;
  SET num_retries = 0;
  SET sleep_interval = wait_interval;

  SELECT IS_AUTOREJOIN_RUNNING() INTO autorejoin_running;

  IF autorejoin_running = TRUE THEN
    -- If -1 is passed as interval, we default to 5 mins (300 secs)
    IF sleep_interval = -1 THEN
      SET sleep_interval = 300;
    END IF;

    -- We obtain the number of retries so far, so we can calculate the time
    -- that has been spent up until the last retry.
    SELECT GET_NUMBER_RETRIES() INTO num_retries;
    SET num_retries = num_retries - 1;

    -- We then obtain the time spent up until now and scale it to seconds.
    SELECT TIMER_WAIT FROM performance_schema.events_stages_current
      WHERE EVENT_NAME LIKE '%auto-rejoin%'
      INTO time_in_secs;
    SET time_in_secs = time_in_secs * 10e-13;

    -- We then estimate the remaining time in seconds. Note: 300sec = 5 mins.
    SET ret = sleep_interval - (time_in_secs - sleep_interval * num_retries);
  END IF;

  RETURN ret;
END //

CREATE FUNCTION GET_LAST_AUTOREJOIN()
  RETURNS INT
BEGIN
  DECLARE ret INT;
  DECLARE num_autorejoins INT;
  SET ret = -1;
  SET num_autorejoins = 0;

  -- We first verify if there have been any auto-rejoin up until now.
  SELECT GET_COUNT_AUTOREJOIN() INTO num_autorejoins;
  IF num_autorejoins > 0 THEN
    SELECT
      UNIX_TIMESTAMP() - ((SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME='UPTIME') - TIMER_END * 10e-13)
      FROM performance_schema.events_stages_history_long
      WHERE EVENT_NAME LIKE '%auto-rejoin%'
      ORDER BY TIMER_END DESC
      LIMIT 1
      INTO ret;
  END IF;

  RETURN ret;
END //

CREATE FUNCTION GET_COUNT_AUTOREJOIN()
  RETURNS INT
BEGIN
  DECLARE ret INT;
  SET ret = 0;

  SELECT COUNT_STAR
    FROM performance_schema.events_stages_summary_global_by_event_name
    WHERE EVENT_NAME LIKE '%auto-rejoin%'
    INTO ret;

  RETURN ret;
END //
DELIMITER ;//

SET SESSION sql_log_bin = 1;
--enable_query_log

--let $include_filename = gr_autorejoin_monitoring.inc
--source include/end_include_file.inc
