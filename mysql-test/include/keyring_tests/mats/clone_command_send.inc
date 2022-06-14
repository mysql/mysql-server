## Send Clone command

# These variables have to be set before sourcing this file. Currently we used
# to test both local and remote clone.
#
# 1. Clone command is expected to return error
# --let clone_err= <error number>
#
# 2. Test Remote Clone command. Default is local clone.
# --let remote_clone = 1
#
# 3. Test clone automatic tuning of threads
# --let clone_auto_tune = 1
#

if ($clone_err != ER_PLUGIN_IS_NOT_LOADED) {
  if (!$clone_auto_tune) {
    SET GLOBAL clone_autotune_concurrency = OFF;
    SET GLOBAL clone_max_concurrency = 8;
  }
  if($remote_clone) {
    if(!$skip_donor_config) {
      --replace_result $HOST HOST $PORT PORT
      --eval SET GLOBAL clone_valid_donor_list = '$HOST:$PORT'
    }
  }
}

if($remote_clone) {
  # Execute Remote Clone command
  --replace_result $CLONE_DATADIR CLONE_DATADIR $HOST HOST $PORT PORT $USER USER
  --send_eval CLONE INSTANCE FROM $USER@$HOST:$PORT IDENTIFIED BY '' DATA DIRECTORY = '$CLONE_DATADIR'
}

if (!$remote_clone) {
  # Execute Local Clone command
  --replace_result $CLONE_DATADIR CLONE_DATADIR
  --send_eval CLONE LOCAL DATA DIRECTORY = '$CLONE_DATADIR'
}
