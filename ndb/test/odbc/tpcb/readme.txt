'tpcb' requires an .odbc.ini file in
/etc/
or in
/home/user/

The .odbc.ini file must contain a DSN entry called ndb:

#--------- .odbc.ini example --------------------

[ndb]
Driver = /path_to_installation/lib/libNDB_ODBC.so

#--------- End of example -----------------------


