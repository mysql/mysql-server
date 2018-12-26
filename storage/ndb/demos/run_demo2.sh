#!/bin/sh
if [ -z "$MYSQLCLUSTER_TOP" ]; then
  echo "MYSQLCLUSTER_TOP not set"
  exit 1
fi
if [ -d "$MYSQLCLUSTER_TOP/ndb" ]; then :; else
  echo "$MYSQLCLUSTER_TOP/ndb directory does not exist"
  exit 1
fi
NDB_CONNECTSTRING=
NDB_HOME=
ndb_demo=$MYSQLCLUSTER_TOP/ndb/demos

# Edit file system path

cd $ndb_demo/2-node/2-mgm-1
sed -e s,"WRITE_PATH_TO_FILESYSTEM_2_HERE",$ndb_demo/2-node/2-db-2/filesystem,g \
    -e s,"WRITE_PATH_TO_FILESYSTEM_3_HERE",$ndb_demo/2-node/2-db-3/filesystem,g \
    < template_config.ini > config.ini

# Start management server as deamon

cd $ndb_demo/2-node/2-mgm-1
if mgmtsrvr -d -c config.ini ; then :; else
  echo "Unable to start mgmtsrvr"
  exit 1
fi

#xterm -T "Demo 2 NDB Management Server" -geometry 80x10 -xrm *.hold:true -e mgmtsrvr -c config.ini &

# Start database node 

cd $ndb_demo/2-node/2-db-2
xterm -T "Demo 2 NDB Cluster DB Node 2" -geometry 80x10 -xrm *.hold:true -e ndb -i &

# Start database node 

cd $ndb_demo/2-node/2-db-3
xterm -T "Demo 2 NDB Cluster DB Node 3"  -geometry 80x10 -xrm *.hold:true -e ndb -i &

# Start xterm for application programs

cd $ndb_demo/2-node/2-api-4
xterm -T "Demo 2 NDB Cluster API Node 4" -geometry 80x10 &

# Start xterm for application programs

cd $ndb_demo/2-node/2-api-5
xterm -T "Demo 2 NDB Cluster API Node 5" -geometry 80x10 &

# Start management client

cd $ndb_demo
xterm -T "Demo 2 NDB Management Client" -geometry 80x10 -xrm *.hold:true -e mgmtclient localhost 10000 &
