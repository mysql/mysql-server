echo $NDB_HOST $NDB_EXTHOST

NDB_PORT=$NDB_PORT_BASE"00"
NDB_CONNECTSTRING_BASE="host=$NDB_HOST:$NDB_PORT;nodeid="

# Edit file system path

cd $NDB_DEMO
sed -e s,"WRITE_PATH_TO_FILESYSTEM_2_HERE",$NDB_DEMO/filesystem,g \
    -e s,"CHOOSE_HOSTNAME",$NDB_HOST,g\
    -e s,"CHOOSE_EXTHOSTNAME",$NDB_EXTHOST,g\
    -e s,"CHOOSE_PORT_BASE",$NDB_PORT_BASE,g\
    -e s,"CHOOSE_REP_ID",$NDB_REP_ID,g\
    -e s,"CHOOSE_EXTREP_ID",$NDB_EXTREP_ID,g\
    < ../config-templates/config_template-1-REP.ini > config.ini

# Start management server as deamon

NDB_ID="1"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
export NDB_CONNECTSTRING
if mgmtsrvr -d -c config.ini ; then :; else
  echo "Unable to start mgmtsrvr"
  exit 1
fi

# Start database node 

NDB_ID="2"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
export NDB_CONNECTSTRING
xterm -T "$NDB_DEMO_NAME DB Node $NDB_ID" -geometry 80x10 -xrm *.hold:true -e ndb -i &

# Start xterm for application programs

NDB_ID="3"
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
export NDB_CONNECTSTRING
xterm -T "$NDB_DEMO_NAME API Node $NDB_ID" -geometry 80x10 &

# Start xterm for rep node

NDB_ID=$NDB_REP_ID
NDB_CONNECTSTRING=$NDB_CONNECTSTRING_BASE$NDB_ID
export NDB_CONNECTSTRING
xterm -T "$NDB_DEMO_NAME REP Node $NDB_ID" -geometry 80x10 -xrm *.hold:true -e ndb_rep &

# Start management client

xterm -T "$NDB_DEMO_NAME Mgmt Client" -geometry 80x10 -xrm *.hold:true -e mgmtclient $NDB_HOST $NDB_PORT &
