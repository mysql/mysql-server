## Clone command test

# These variables can to be set before sourcing this file. Currently we used
# to test both local and remote clone.
#
# 1. Clone command is expected to return error
# --let clone_err= <error number>
#
# 2. Test Remote Clone command. Default is local clone.
# --let remote_clone = 1
#
# 3. Remote Clone command is expected to return error and the error number
# is different from local clone.
# --let clone_remote_err = <error number>
#
# 4. Skip clone_valid_donor_list configuration for testing error cases
# --let skip_donor_config = 1
#
# 5. Test clone automatic tuning of threads
# --let clone_auto_tune = 1
#
# 6. Test clone command forcing SSL [REQUIRES SSL]
# --let clone_require_ssl = 1
#
# 7. Test clone command forcing insecure connection [REQUIRES NO SSL]
# --let clone_require_no_ssl = 1
#
# 8. Test clone command forcing SSL certificate validation
# --let clone_require_ssl_certificate = 1
#

if ($disabled_redo) {
  ALTER INSTANCE ENABLE INNODB REDO_LOG;
}

--let $remote_dir_clause = DATA DIRECTORY = '$CLONE_DATADIR'

if ($clone_remote_replace) {
  --let $remote_dir_clause =
  --let $remote_clone = 1
}

if ($clone_err != ER_PLUGIN_IS_NOT_LOADED) {
  if (!$clone_auto_tune) {
    SET GLOBAL clone_autotune_concurrency = OFF;
    SET GLOBAL clone_max_concurrency = 8;
  }

  if ($clone_throttle) {
    SET GLOBAL clone_max_data_bandwidth = 5;
    SET GLOBAL clone_max_network_bandwidth = 4;
  }

  if($remote_clone) {
    if(!$skip_donor_config) {
      --replace_result $HOST HOST $PORT PORT
      --eval SET GLOBAL clone_valid_donor_list = '$HOST:$PORT'
    }
  }
}

if($remote_clone) {

  # Increase network timeout for valgrind test
  if ($VALGRIND_TEST) {
    --disable_query_log
    SET GLOBAL net_read_timeout = 300;
    SET LOCAL net_read_timeout = 300;
    SET GLOBAL net_write_timeout = 300;
    SET LOCAL net_write_timeout = 300;
    --enable_query_log
  }

   if ($delay_after_data_drop) {
    # Delay the clone process for 10 seconds
    SET GLOBAL clone_delay_after_data_drop = 2;
   }

  # Execute Remote Clone command with error
  if ($clone_remote_err) {

    --replace_result $CLONE_DATADIR CLONE_DATADIR $HOST HOST $PORT PORT $USER USER
    if ($clone_require_no_ssl) {
      --error $clone_remote_err
      --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause REQUIRE NO SSL
    }

    if(!$clone_require_no_ssl) {
      --error $clone_remote_err
      --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause
    }
  }

  if (!$clone_remote_err) {
    if ($clone_err) {

      if ($clone_err == ER_FILE_EXISTS_ERROR) {

        --replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR $HOST HOST $PORT PORT $USER USER
        --error $clone_err
        --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause
      }

      if ($clone_err != ER_FILE_EXISTS_ERROR) {

        --replace_regex /\([0-9]+\)\./(socket errno)./
        --replace_result $CLONE_DATADIR CLONE_DATADIR $HOST HOST $PORT PORT $USER USER
        --error $clone_err
        --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause
      }
    }

    # Execute Remote Clone command
    if (!$clone_err) {

      # Execute Remote Clone command
      if($clone_require_ssl) {
         if($clone_require_ssl_certificate) {
            SHOW VARIABLES LIKE "%clone_ssl%";
            --replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
            --eval SET GLOBAL clone_ssl_ca = '$MYSQL_TEST_DIR/std_data/cacert.pem'
            --replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
            --eval SET GLOBAL clone_ssl_cert = '$MYSQL_TEST_DIR/std_data/client-cert.pem'
            --replace_result $MYSQL_TEST_DIR MYSQL_TEST_DIR
            --eval SET GLOBAL clone_ssl_key = '$MYSQL_TEST_DIR/std_data/client-key.pem'
         }
         --replace_result $CLONE_DATADIR CLONE_DATADIR $HOST HOST $PORT PORT $USER USER
         --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause REQUIRE SSL
      }

      --replace_result $CLONE_DATADIR CLONE_DATADIR $HOST HOST $PORT PORT $USER USER
      if(!$clone_require_ssl) {
        if ($clone_require_no_ssl) {
          --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause REQUIRE NO SSL
        }

        if(!$clone_require_no_ssl) {
          --eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' $remote_dir_clause
        }
      }
    }
  }

  # For remote replace, wait for server to restart, unless an error was expected
  if ($clone_remote_replace) {
    if (!$clone_err) {
      --source include/wait_until_disconnected.inc
      --enable_reconnect
      --source include/wait_until_connected_again.inc
      --disable_reconnect
      if (!$clone_recovery_error) {
        --disable_query_log
        use test;
        --enable_query_log
      }
    }
  }

  if ($VALGRIND_TEST) {
    --disable_query_log
    SET GLOBAL net_read_timeout = default;
    SET LOCAL net_read_timeout = default;
    SET GLOBAL net_write_timeout = default;
    SET LOCAL net_write_timeout = default;
    --enable_query_log
  }

   if ($delay_after_data_drop) {
    SET GLOBAL clone_delay_after_data_drop = 0;
   }
}

if (!$remote_clone) {

  # Execute Local Clone command with error
  if ($clone_err) {

    if ($clone_err == ER_FILE_EXISTS_ERROR) {

      --replace_result $MYSQL_TMP_DIR MYSQL_TMP_DIR
      --error $clone_err
      --eval CLONE LOCAL DATA DIRECTORY = '$CLONE_DATADIR'
    }

    if ($clone_err != ER_FILE_EXISTS_ERROR) {

      --replace_result $CLONE_DATADIR CLONE_DATADIR
      --error $clone_err
      --eval CLONE LOCAL DATA DIRECTORY = '$CLONE_DATADIR'
    }
  }

  # Execute Local Clone command
  if (!$clone_err) {

    --replace_result $CLONE_DATADIR CLONE_DATADIR
    --eval CLONE LOCAL DATA DIRECTORY = '$CLONE_DATADIR'
  }
}

# PFS views are not available if clone plugin is not installed
if ($clone_err != ER_PLUGIN_IS_NOT_LOADED) {
  if (!$clone_recovery_error) {
    select ID, STATE, ERROR_NO from performance_schema.clone_status;
  }

  if ($clone_recovery_error) {
    select ID, STATE, ERROR_NO, ERROR_MESSAGE from performance_schema.clone_status;
  }

  select ID, STAGE, STATE from performance_schema.clone_progress;

  # For covering the code fetching all columns
  if ($clone_auto_tune) {
    --disable_query_log
    CREATE TABLE dup_status as select * from performance_schema.clone_status;
    CREATE TABLE dup_progress as select * from performance_schema.clone_progress;
    DROP TABLE dup_status;
    DROP TABLE dup_progress;
    --enable_query_log
  }
}

if ($disabled_redo) {
  ALTER INSTANCE DISABLE INNODB REDO_LOG;
}
