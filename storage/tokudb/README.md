TokuDB
======

TokuDB is a high-performance, write optimized, transactional storage engine for MySQL, MariaDB, and Percona Server.
For more details, see our [product page][products].

This repository contains the MySQL plugin that uses the [TokuFT][tokuft] core.

There are also patches to the MySQL and MariaDB kernels, available in our
forks of [mysql][mysql] and [mariadb][mariadb].

[products]: http://www.tokutek.com/products/tokudb-for-mysql/
[tokuft]: http://github.com/Tokutek/ft-index
[mysql]: http://github.com/Tokutek/mysql
[mariadb]: http://github.com/Tokutek/mariadb

Download
--------

* [MySQL 5.5 + TokuDB](http://www.tokutek.com/tokudb-for-mysql/download-community/)
* [MariaDB 5.5 + TokuDB](http://www.tokutek.com/tokudb-for-mysql/download-community/)
* [MariaDB 10.0 + TokuDB](https://downloads.mariadb.org/)
* [Percona Server 5.6 + TokuDB](http://www.percona.com/downloads/)

Build
-----

The `scripts/` directory contains a script that can be used to build a
working MySQL or MariaDB with Tokutek patches, and with the TokuDB storage
engine, called `make.mysql.bash`.  This script will download copies of the
needed source code from github and build everything.

To build MySQL 5.5.41 with TokuDB 7.5.5:
```sh
scripts/make.mysql.bash --mysqlbuild=mysql-5.5.41-tokudb-7.5.5-linux-x86_64
```

To build MariaDB 5.5.41 with TokuDB 7.5.5:
```sh
scripts/make.mysql.bash --mysqlbuild=mariadb-5.5.41-tokudb-7.5.5-linux-x86_64
```

Before you start, make sure you have a C++11-compatible compiler (GCC >=
4.7 is recommended), as well as CMake >=2.8.8, and the libraries and
header files for valgrind,zlib, and Berkeley DB.  We are using the gcc 4.7
in devtoolset-1.1.

On CentOS, `yum install valgrind-devel zlib-devel libdb-devel`

On Ubuntu, `apt-get install valgrind zlib1g-dev libdb-dev`

You can set the compiler by passing `--cc` and `--cxx` to the script, to
select one that's new enough.  The default is `scripts/make.mysql.bash
--cc=gcc47 --cxx=g++47`, which may not exist on your system.

To build a debug MySQL with TokuDB using the head of the Tokutek github
repositories, run this:
```sh
scripts/make.mysql.debug.env.bash
```

We use gcc from devtoolset-1.1 on CentOS 5.9 for builds.

Contribute
----------

Please report TokuDB bugs at https://tokutek.atlassian.net/browse/DB.

We have two publicly accessible mailing lists:

 - tokudb-user@googlegroups.com is for general and support related
   questions about the use of TokuDB.
 - tokudb-dev@googlegroups.com is for discussion of the development of
   TokuDB.

We are on IRC on freenode.net, in the #tokutek channel.


License
-------

TokuDB is available under the GPL version 2.  See [COPYING][copying]

The TokuFT component of TokuDB is available under the GPL version 2, with
slight modifications.  See [README-TOKUDB][license].

[copying]: http://github.com/Tokutek/tokudb-engine/blob/master/COPYING
[license]: http://github.com/Tokutek/tokudb-index/blob/master/README-TOKUDB
