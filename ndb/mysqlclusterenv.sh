# Sets necessary environment variables for mysqlcluster install scripts
mytop=
if [ -f bin/mysql ]; then
	mytop=`/bin/pwd`
elif [ -f bin/ndb ]; then
	mytop=`dirname \`/bin/pwd\``
fi
if [ "$mytop" ]; then
	MYSQLCLUSTER_TOP=$mytop
	PATH=$MYSQLCLUSTER_TOP/bin:$MYSQLCLUSTER_TOP/ndb/bin:$PATH
	LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/lib:$LD_LIBRARY_PATH
	LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/ndb/lib:$LD_LIBRARY_PATH
	export MYSQLCLUSTER_TOP PATH LD_LIBRARY_PATH
else
if [ -d SCCS ]; then
if [ -f ndb/mysqlclusterenv.sh ]; then
	mytop=`/bin/pwd`
elif [ -f mysqlclusterenv.sh ]; then
	mytop=`dirname \`/bin/pwd\``
fi
fi
if [ "$mytop" ]; then
# we're in the development tree
    if [ "$REAL_EMAIL" ]; then :; else
#Guessing REAL_EMAIL
    REAL_EMAIL=`whoami`@mysql.com
    export REAL_EMAIL
    echo Setting REAL_EMAIL=$REAL_EMAIL
    fi

    MYSQLCLUSTER_TOP=$mytop

    NDB_TOP=$MYSQLCLUSTER_TOP/ndb
    export NDB_TOP

    NDB_PROJ_HOME=$NDB_TOP/home
    export NDB_PROJ_HOME

    PATH=$MYSQLCLUSTER_TOP/ndb/bin:$MYSQLCLUSTER_TOP/ndb/home/bin:$PATH
    PATH=$MYSQLCLUSTER_TOP/client:$PATH
    PATH=$MYSQLCLUSTER_TOP/sql:$PATH
    LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/libmysql:$LD_LIBRARY_PATH
    LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/libmysqld:$LD_LIBRARY_PATH
    LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/ndb/lib:$LD_LIBRARY_PATH
    LD_LIBRARY_PATH=$MYSQLCLUSTER_TOP/libmysql_r/.libs:$LD_LIBRARY_PATH
    export MYSQLCLUSTER_TOP PATH LD_LIBRARY_PATH
else
	echo "Please source this file (mysqlclusterenv.sh) from installation top directory"
fi
fi
mytop=
