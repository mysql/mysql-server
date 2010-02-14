#!/bin/bash
source env.properties
echo MYSQL_HOME=$MYSQL_HOME

echo
"$MYSQL_BIN/ndb_mgm" -e show

# retry reloading the schema
# seems that dropping the tables fails if
# - the data nodes haven't fully come up yet
# - ndb and mysqld have gotten out of sync (e.g., may happen when ndb was
#   (re)started with option "--initial", see bug.php?id=42107)
for ((i=5; i>=0; i--)) ; do

  echo
  echo reload schema...
  "$MYSQL_BIN/mysql" -v -u root < src/tables_mysql.sql
#  "$MYSQL_HOME/bin/mysql" -v < src/tables_mysql0.sql
  s=$?
  echo mysql exit status: $s

  if [[ $s == 0 ]]; then
    echo "successfully (re)loaded schema"
    break
  else
    echo
    echo "failed (re)loading schema"
    echo "retrying up to $i times (please be patient)..."
    for ((j=0; j<5; j++)) ; do echo "." ; sleep 1; done
    "$MYSQL_BIN/mysql" -e "USE crunddb; SHOW TABLES;" > /dev/null
    for ((j=0; j<5; j++)) ; do echo "." ; sleep 1; done
  fi

done

echo
echo "show tables..."
"$MYSQL_BIN/mysql" -e "USE crunddb; SHOW TABLES;"

echo
echo done.
