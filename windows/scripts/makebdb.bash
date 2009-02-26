#!/bin/bash
mkdir -p /usr/local/BerkeleyDB.$1/bin
mkdir -p /usr/local/BerkeleyDB.$1/lib
ln -s /usr/bin/db$1_load.exe /usr/local/BerkeleyDB.$1/bin/db_load
ln -s /usr/bin/db$1_dump.exe /usr/local/BerkeleyDB.$1/bin/db_dump
ln -s /usr/include/db /usr/local/BerkeleyDB.$1/include
ln -s /usr/lib/libdb-.a /usr/local/BerkeleyDB.$1/lib/libdb.a
ln -s /usr/lib/libdb-.dll.a /usr/local/BerkeleyDB.$1/lib/libdb.dll.a
ln -s /usr/lib/libdb-.la /usr/local/BerkeleyDB.$1/lib/libdb.la
ln -s /usr/lib/libdb_cxx-.la /usr/local/BerkeleyDB.$1/lib/libdb_cxx.la
ln -s /usr/lib/libdb_cxx-.a /usr/local/BerkeleyDB.$1/lib/libdb_cxx.a
ln -s /usr/lib/libdb_cxx-.dll.a /usr/local/BerkeleyDB.$1/lib/libdb_cxx.dll.a
