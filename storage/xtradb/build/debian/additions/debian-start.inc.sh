#!/bin/bash
#
# This file is included by /etc/mysql/debian-start
#

## Check all unclosed tables.
# - Requires the server to be up.
# - Is supposed to run silently in background. 
function check_for_crashed_tables() {
  set -e
  set -u

  # But do it in the background to not stall the boot process.
  logger -p daemon.info -i -t$0 "Triggering myisam-recover for all MyISAM tables"

  # Checking for $? is unreliable so the size of the output is checked.
  # Some table handlers like HEAP do not support CHECK TABLE.
  tempfile=`tempfile`
  # We have to use xargs in this case, because a for loop barfs on the 
  # spaces in the thing to be looped over. 
  LC_ALL=C $MYSQL --skip-column-names --batch -e  '
      select concat("select count(*) into @discard from `",
                    TABLE_SCHEMA, "`.`", TABLE_NAME, "`") 
      from information_schema.TABLES where ENGINE="MyISAM"' | \
    xargs -i $MYSQL --skip-column-names --silent --batch \
                    --force -e "{}" >$tempfile 
  if [ -s $tempfile ]; then
    (
      /bin/echo -e "\n" \
        "Improperly closed tables are also reported if clients are accessing\n" \
 	"the tables *now*. A list of current connections is below.\n";
       $MYADMIN processlist status
    ) >> $tempfile
    # Check for presence as a dependency on mailx would require an MTA.
    if [ -x /usr/bin/mailx ]; then 
      mailx -e -s"$MYCHECK_SUBJECT" $MYCHECK_RCPT < $tempfile 
    fi
    (echo "$MYCHECK_SUBJECT"; cat $tempfile) | logger -p daemon.warn -i -t$0
  fi
  rm $tempfile
}

## Check for tables needing an upgrade.
# - Requires the server to be up.
# - Is supposed to run silently in background. 
function upgrade_system_tables_if_necessary() {
  set -e
  set -u

  logger -p daemon.info -i -t$0 "Upgrading MySQL tables if necessary."

  # Filter all "duplicate column", "duplicate key" and "unknown column"
  # errors as the script is designed to be idempotent.
  LC_ALL=C $MYUPGRADE \
    2>&1 \
    | egrep -v '^(1|@had|ERROR (1054|1060|1061))' \
    | logger -p daemon.warn -i -t$0
}

## Check for the presence of both, root accounts with and without password.
# This might have been caused by a bug related to mysql_install_db (#418672).
function check_root_accounts() {
  set -e
  set -u
  
  logger -p daemon.info -i -t$0 "Checking for insecure root accounts."

  ret=$( echo "SELECT count(*) FROM mysql.user WHERE user='root' and password='';" | $MYSQL --skip-column-names )
  if [ "$ret" -ne "0" ]; then
    logger -p daemon.warn -i -t$0 "WARNING: mysql.user contains $ret root accounts without password!"
  fi
}
