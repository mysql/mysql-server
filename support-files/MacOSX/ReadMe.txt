
2.5. Installing MySQL on Mac OS X

   You can install MySQL on Mac OS X 10.3.x ("Panther") or newer
   using a Mac OS X binary package in PKG format instead of the
   binary tarball distribution. Please note that older versions of
   Mac OS X (for example, 10.1.x or 10.2.x) are not supported by this
   package.

   The package is located inside a disk image (.dmg) file that you
   first need to mount by double-clicking its icon in the Finder. It
   should then mount the image and display its contents.

   To obtain MySQL, see Section 2.1.3, "How to Get MySQL."

Note

   Before proceeding with the installation, be sure to shut down all
   running MySQL server instances by either using the MySQL Manager
   Application (on Mac OS X Server) or via mysqladmin shutdown on the
   command line.

   To actually install the MySQL PKG file, double-click on the
   package icon. This launches the Mac OS X Package Installer, which
   guides you through the installation of MySQL.

   Due to a bug in the Mac OS X package installer, you may see this
   error message in the destination disk selection dialog:
You cannot install this software on this disk. (null)

   If this error occurs, simply click the Go Back button once to
   return to the previous screen. Then click Continue to advance to
   the destination disk selection again, and you should be able to
   choose the destination disk correctly. We have reported this bug
   to Apple and it is investigating this problem.

   The Mac OS X PKG of MySQL installs itself into
   /usr/local/mysql-VERSION and also installs a symbolic link,
   /usr/local/mysql, that points to the new location. If a directory
   named /usr/local/mysql exists, it is renamed to
   /usr/local/mysql.bak first. Additionally, the installer creates
   the grant tables in the mysql database by executing
   mysql_install_db.

   The installation layout is similar to that of a tar file binary
   distribution; all MySQL binaries are located in the directory
   /usr/local/mysql/bin. The MySQL socket file is created as
   /tmp/mysql.sock by default. See Section 2.1.5, "Installation
   Layouts."

   MySQL installation requires a Mac OS X user account named mysql. A
   user account with this name should exist by default on Mac OS X
   10.2 and up.

   If you are running Mac OS X Server, a version of MySQL should
   already be installed. The following table shows the versions of
   MySQL that ship with Mac OS X Server versions.
   Mac OS X Server Version MySQL Version
   10.2-10.2.2             3.23.51
   10.2.3-10.2.6           3.23.53
   10.3                    4.0.14
   10.3.2                  4.0.16
   10.4.0                  4.1.10a

   This manual section covers the installation of the official MySQL
   Mac OS X PKG only. Make sure to read Apple's help information
   about installing MySQL: Run the "Help View" application, select
   "Mac OS X Server" help, do a search for "MySQL," and read the item
   entitled "Installing MySQL."

   If you previously used Marc Liyanage's MySQL packages for Mac OS X
   from http://www.entropy.ch, you can simply follow the update
   instructions for packages using the binary installation layout as
   given on his pages.

   If you are upgrading from Marc's 3.23.x versions or from the Mac
   OS X Server version of MySQL to the official MySQL PKG, you also
   need to convert the existing MySQL privilege tables to the current
   format, because some new security privileges have been added. See
   Section 4.4.8, "mysql_upgrade --- Check Tables for MySQL Upgrade."

   If you want MySQL to start automatically during system startup,
   you also need to install the MySQL Startup Item. It is part of the
   Mac OS X installation disk images as a separate installation
   package. Simply double-click the MySQLStartupItem.pkg icon and
   follow the instructions to install it. The Startup Item need be
   installed only once. There is no need to install it each time you
   upgrade the MySQL package later.

   The Startup Item for MySQL is installed into
   /Library/StartupItems/MySQLCOM. (Before MySQL 4.1.2, the location
   was /Library/StartupItems/MySQL, but that collided with the MySQL
   Startup Item installed by Mac OS X Server.) Startup Item
   installation adds a variable MYSQLCOM=-YES- to the system
   configuration file /etc/hostconfig. If you want to disable the
   automatic startup of MySQL, simply change this variable to
   MYSQLCOM=-NO-.

   On Mac OS X Server, the default MySQL installation uses the
   variable MYSQL in the /etc/hostconfig file. The MySQL Startup Item
   installer disables this variable by setting it to MYSQL=-NO-. This
   avoids boot time conflicts with the MYSQLCOM variable used by the
   MySQL Startup Item. However, it does not shut down a running MySQL
   server. You should do that yourself.

   After the installation, you can start up MySQL by running the
   following commands in a terminal window. You must have
   administrator privileges to perform this task.

   If you have installed the Startup Item, use this command:
shell> sudo /Library/StartupItems/MySQLCOM/MySQLCOM start
(Enter your password, if necessary)
(Press Control-D or enter "exit" to exit the shell)

   If you don't use the Startup Item, enter the following command
   sequence:
shell> cd /usr/local/mysql
shell> sudo ./bin/mysqld_safe
(Enter your password, if necessary)
(Press Control-Z)
shell> bg
(Press Control-D or enter "exit" to exit the shell)

   You should be able to connect to the MySQL server, for example, by
   running /usr/local/mysql/bin/mysql.

Note

   The accounts that are listed in the MySQL grant tables initially
   have no passwords. After starting the server, you should set up
   passwords for them using the instructions in Section 2.11,
   "Post-Installation Setup and Testing."

   You might want to add aliases to your shell's resource file to
   make it easier to access commonly used programs such as mysql and
   mysqladmin from the command line. The syntax for bash is:
alias mysql=/usr/local/mysql/bin/mysql
alias mysqladmin=/usr/local/mysql/bin/mysqladmin

   For tcsh, use:
alias mysql /usr/local/mysql/bin/mysql
alias mysqladmin /usr/local/mysql/bin/mysqladmin

   Even better, add /usr/local/mysql/bin to your PATH environment
   variable. You can do this by modifying the appropriate startup
   file for your shell. For more information, see Section 4.2.1,
   "Invoking MySQL Programs."

   If you are upgrading an existing installation, note that
   installing a new MySQL PKG does not remove the directory of an
   older installation. Unfortunately, the Mac OS X Installer does not
   yet offer the functionality required to properly upgrade
   previously installed packages.

   To use your existing databases with the new installation, you'll
   need to copy the contents of the old data directory to the new
   data directory. Make sure that neither the old server nor the new
   one is running when you do this. After you have copied over the
   MySQL database files from the previous installation and have
   successfully started the new server, you should consider removing
   the old installation files to save disk space. Additionally, you
   should also remove older versions of the Package Receipt
   directories located in /Library/Receipts/mysql-VERSION.pkg.
