The *_clean.zip files represent clean data directories created by different versions of MySQL server.
These zips are decompressed and redo log files are modified, in the innodb.log_corruption test.

In order to regenerate these zip files a following procedure needs to be used for each of them:

1. Checkout the proper version of source (mysql-8.0.19-release, mysql-8.0.11-release, mysql-5.7.9-release,
   mysql-5.7.8 or mysql-5.6).

2. Configure the build directory for release build, for example:
    cd build/8.0.19-build
    cmake ../../git/8.0.19-src -DWITH_DEBUG=0 -DWITH_BOOST=../../boost/ -DDOWNLOAD_BOOST=1

3. Build the mysqld, for example:
    make -j8 mysqld

4. Run the mysqld to initialize new data directory, prodiving --initialize-insecure option
   and (IMPORTANT !) --lower_case_table_names=1, which is important for executions of test
   on Windows (datadir needs to be compatible for different OS!). Also, consider passing
   --innodb_log_file_size=4M just to minimize the size of datadir after decompression.

   An examle for < 8.x:
      ./sql/mysqld --initialize-insecure --datadir=/tmp/5_7_9 --console --lower_case_table_names=1 --innodb_log_file_size=4M

   An example for >= 8.0.11:
      ./bin/mysqld --initialize-insecure --datadir=/tmp/8_0_11 --console --lower_case_table_names=1 --innodb_log_file_size=4M

5. Start the mysqld on the created data directory, providing --innodb-fast-shutdown=0.

      ./bin/mysqld --datadir=/tmp/8_0_11 --innodb-fast-shutdown=0 --lower_case_table_names=1 --innodb_log_file_size=4M

      (for 5.6, 5.7.8, 5.7.9 use ./sql/mysqld and provide also: --lc-messages-dir=./sql/share/english)

6. Stop the mysqld (pkill mysqld).

7. Compress the contents of the created data directory (for example what is inside /tmp/5_7_9)
   and replace the zip file in this directory with the created one (e.g. 5_7_9_clean.zip).

EXCEPTION:

For mysql-5.6, it is more tricky:

In point 3: make -j8 is recommended to build all targets.

In point 4: you need to use ./scripts/mysql_install_db perl script to create data directory, for example:

  perl ./scripts/mysql_install_db --cross-bootstrap --user=mysql --builddir=. --srcdir=../../git/8.0 \
     --datadir=/tmp/5_6 --lc-messages-dir=./sql/share/english --lower-case-table-names=1 --innodb-log-file-size=4M

Note, the --cross-bootstrap used to generate data directory which should be ok for different hosts.