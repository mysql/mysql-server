====================================================

Contents
--------
Welcome to the latest port of MySQL for OS/2 and eComStation.

Modules included in this build:
	- protocol data compression
	- perl BDB/BDI support (not in this package)
	- Library and header files for C/CPP developers included

This package has been built using IBM VAC++ 4.0

The MySQL server is distributed under the GPL license. Please refer to
the file COPYING for the license information.

The MySQL client library is distributed under the LGPL license.
Please refer to the file COPYING for the license information.

Most of the MySQL clients are distributed under the GPL license, but
some files may be in the public domain.

The latest information about MySQL can be found at: http://www.mysql.com

To get the latest information about this port please subscribe to our
newsgroup/mailinglist mysql2 at groups.yahoo.com.

To see what MySQL can do, take a look at the features section in the
manual. For future plans see the TODO appendix in the manual.

New features/bug fixes history is in the news appendix in the manual.

For the currently known bugs/misfeatures (known errors) see the bugs
appendix in the manual.  The OS/2 section contains notes that are
specific to the MySQL OS/2 and eComStation version.

Please note that MySQL is a constantly moving target. New builds for
Linux are made available every week. This port may therefore be a few
minor versions after the latest Linux/Win32 builds but its generally
more stable than the "latest and greates" port.

MySQL is brought to you by: TcX DataKonsult AB & MySQL Finland AB

This port is brought to you by:

Yuri Dario <mc6530@mclink.it>, development, porting
Timo Maier <tam@gmx.de>, documentation, testing
John M Alfredsson <jma@jmast.se>, documentation, testing


Installation
------------
Prerequisite:

- OS/2 Warp 3 with FP ?? or later, 
  OS/2 Warp 4 with FP ?? or later,
  OS/2 Warp Server for e-Business,
  eComStation 1.0 (prev 1/2 OK)
- TCPIP 4.x installed (requires 32-bit tcpip stack)
- WarpIN installer 0.9.16 (ftp://ftp.os2.org/xworkplace/warpin-0-9-16.exe)

Note: probably some fixpak level is required on both Warp3&Warp4 to
      support >2GB file sizes.

Save the installation archives into a temporary folder and double click 
on the main package; otherwise you can drop the mysql package in your 
WarpIN object or type 

	WARPIN MYSQL-3-23-??-B1.WPI 

from the command line.
The configuration file for MySQL is named my.cnf and it is placed into
your %ETC% directory. Usually it located into the boot driver under

	x:\MPTN\ETC

If the installation detect an existing configuration file, it will not be
overwritten, keeping you settings; see x:\...\mysql\data\my.cnf.sample
for new settings. This file is not deleted by uninstall process.
Startup options for MySQL daemon could be added there.

As default, client connections uses data compression: if you don't like it,
remove the following from your %ETC%\my.cnf

	[client]
	compress

The server switches automatically compression mode on client request.

This release comes with DLL client library MYSQL.DLL: it is installed by
default into mysql\bin together with client applications. Copy it to your
x:\OS2\DLL or another directory in your LIBPATH to run command line 
utilities from every place.

See documentation for manuals installation.


New features
------------
With build 4, the sql daemon supports a new option

	--preload-client-dll

that enables preloading of mysql.dll and mysqlu.dll directly by the
server. This way, client programs doesn't need to have the dll's in
the current libpath.


Documentation
-------------
Documentation is provided in separate files. You can use either
the PDF documentation (requires Adobe Acrobat Reader) or the
INF documentation (requires OS/2 view or NewView).

The PDF documentation is found in 

		MYSQL-3-23-??-PDF.WPI 

and the INF documentation is found in 

		MYSQL-3-23-28-INF.WPI

The latest documentation in other formats can always be downloaded from 
http://www.mysql.com. However this documentation may not fully apply to 
this port.
The INF documentation could contain errors because of semi-automatic
translation from texi original. Also it is not updated as the latest PDF
manual (sorry, but conversion from texi to ipf requires quite a lot of 
work).
To install the manuals, their WPI must be placed in the same directory
of the main WPI package: once the main package installation is started,
new install options will be available (inf or pdf manual).


Support
-------
Since MySQL is a OpenSource freeware product there are no
formal support options available.

Please subscribe to mysql2 at www.yahoogroups.com to get in contact
with other users using this port.

http://www.yahoogroups.com/group/mysql2

This newsgroup/mailinglist is the official "home" of this port.


Donations
---------
Since this software is ported for free, donations are welcome!
You can get also an extended support, which is not free and subject to
custom rates.
Ask in the mailing list for details.
At least, a post card is welcome!


Know problems
-------------
alter_table.test and show_check are failing, reporting a different status 
message: actually seems only a different text, no bugs in table checking.


Apache/2 + PHP
--------------
To avoid problems with different socket when you use PHP and Apache
webserver, get the PHP4 module from the Apache Server for OS/2 homepage
http://silk.apana.org.au/apache/ 


Developing MySQL
----------------
If you want to help us develop MySQL for OS2/eComStation please join
the mysql2 mailinglist at www.egroups.com and ask for help to set up 
your environment!

All questions that are specific to the OS2/eComStation version should 
be posted to this list!  Please remember to include all relevant
information that may help solve your problem.


Building MySQL (VAC++ 4)
------------------------
Place zlib-1.1.4 at the same level of mysql-3.23.50
Place ufc lib at the same level of mysql-3.23.50

Add the following files:
	include\config-os2.h
	include\mysql_version.h
	mysys\my_os2*.*
Get the following files from Windows source distribution:
	strings\ctype_extra_sources.c
	libmysql\dll.c

Apply file and patches found in the src\ directory (if exists).
Create the following subdirectories

	bin\
	bin\test
	lib\
	obj\
	obj\zlib

Build os2\MySQL-Client.icc project first.
Then os2\MySQL-Util.icc; last is os2\MySQL-Sql.icc

