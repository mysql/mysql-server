#!/bin/bash

echo "Updating optimizer statistics..."

#/usr/bin/vacuumdb -z $SID
/usr/bin/psql $SID -c "analyze supplier"
/usr/bin/psql $SID -c "analyze part"
/usr/bin/psql $SID -c "analyze partsupp"
/usr/bin/psql $SID -c "analyze customer"
/usr/bin/psql $SID -c "analyze orders"
/usr/bin/psql $SID -c "analyze lineitem"
/usr/bin/psql $SID -c "analyze nation"
/usr/bin/psql $SID -c "analyze region"
