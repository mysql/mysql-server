
2.7. Installing MySQL on Mac OS X

   MySQL for Mac OS X is available in a number of different forms:

     * Native Package Installer format, which uses the native Mac OS
       X installer to walk you through the installation of MySQL. For
       more information, see Section 2.7.1, "Installing MySQL Using
       the Installation Package." You can use the package installer
       with Mac OS X 10.3 and later, and available for both PowerPC
       and Intel architectures, and both 32-bit and 64-bit
       architectures. There is no Universal Binary available using
       the package installation method. The user you use to perform
       the installation must have administrator privileges.

     * Tar package format, which uses a file packaged using the Unix
       tar and gzip commands. To use this method, you will need to
       open a Terminal window. You do not need administrator
       privileges using this method, as you can install the MySQL
       server anywhere using this method. For more information on
       using this method, you can use the generic instructions for
       using a tarball, Section 2.2, "Installing MySQL from Generic
       Binaries on Unix/Linux."You can use the package installer with
       Mac OS X 10.3 and later, and available for both PowerPC and
       Intel architectures, and both 32-bit and 64-bit architectures.
       A Universal Binary, incorporating both Power PC and Intel
       architectures and 32-bit and 64-bit binaries is available.
       In addition to the core installation, the Package Installer
       also includes Section 2.7.2, "Installing the MySQL Startup
       Item" and Section 2.7.3, "Installing and Using the MySQL
       Preference Pane," both of which simplify the management of
       your installation.

     * Mac OS X server includes a version of MySQL as standard. If
       you want to use a more recent version than that supplied with
       the Mac OS X server release, you can make use of the package
       or tar formats. For more information on using the MySQL
       bundled with Mac OS X, see Section 2.7.4, "Using MySQL on Mac
       OS X Server."

   For additional information on using MySQL on Mac OS X, see Section
   2.7.5, "MySQL Installation on Mac OS X Notes."

2.7.1. Installing MySQL Using the Installation Package

   You can install MySQL on Mac OS X 10.3.x ("Panther") or newer
   using a Mac OS X binary package in PKG format instead of the
   binary tarball distribution. Please note that older versions of
   Mac OS X (for example, 10.1.x or 10.2.x) are not supported by this
   package.

   The package is located inside a disk image (.dmg) file that you
   first need to mount by double-clicking its icon in the Finder. It
   should then mount the image and display its contents.

Note

   Before proceeding with the installation, be sure to shut down all
   running MySQL server instances by either using the MySQL Manager
   Application (on Mac OS X Server) or via mysqladmin shutdown on the
   command line.

   When installing from the package version, you should also install
   the MySQL Preference Pane, which will allow you to control the
   startup and execution of your MySQL server from System
   Preferences. For more information, see Section 2.7.3, "Installing
   and Using the MySQL Preference Pane."

   When installing using the package installer, the files are
   installed into a directory within /usr/local matching the name of
   the installation version and platform. For example, the installer
   file mysql-5.1.39-osx10.5-x86_64.pkg installs MySQL into
   /usr/local/mysql-5.1.39-osx10.5-x86_64 . The installation layout
   of the directory is as shown in the following table:
   Directory       Contents of Directory
   bin             Client programs and the mysqld server
   data            Log files, databases
   docs            Manual in Info format
   include         Include (header) files
   lib             Libraries
   man             Unix manual pages
   mysql-test      MySQL test suite
   scripts         Contains the mysql_install_db script
   share/mysql     Error message files
   sql-bench       Benchmarks
   support-files   Scripts and sample configuration files
   /tmp/mysql.sock The location of the MySQL Unix socket

   During the package installer process, a symbolic link from
   /usr/local/mysql to the version/platform specific directory
   created during installation will be created automatically.

    1. Download and open the MySQL package installer, which is
       provided on a disk image (.dmg). Double-click to open the disk
       image, which includes the main MySQL installation package, the
       MySQLStartupItem.pkg installation package, and the
       MySQL.prefPane.

    2. Double-click on the MySQL installer package. It will be named
       according to the version of MySQL you have downloaded. For
       example, if you have downloaded MySQL 5.1.39, double-click
       mysql-5.1.39-osx10.5-x86.pkg.

    3. You will be presented with the openin installer dialog. Click
       Continue to begihn installation.
       MySQL Package Installer: Step 1

    4. A copy of the installation instructions and other important
       information relevant to this installation are display. Click
       Continue .

    5. If you have downloaded the community version of MySQL, you
       will be shown a copy of the relevent GNU General Public
       License. Click Continue .

    6. Select the drive you want to use to install the MySQL Startup
       Item. The drive must have a valid, bootable, Mac OS X
       operating system installed. Click Continue.
       MySQL Package Installer: Step 4

    7. You will be asked to confirm the details of the installation,
       including the space required for the installation. To change
       the drive on which the startup item is installed you can click
       either Go Back or Change Install Location.... To install the
       startup item, click Install.

    8. Once the installation has been completed successfully, you
       will be given an Install Succeeded message.

   Once you have completed the basic installation, you must complete
   the post-installation steps as specifed in Section 2.13,
   "Post-Installation Setup and Testing."

   For convenience, you may also want to install the Section 2.7.2,
   "Installing the MySQL Startup Item" and Section 2.7.3, "Installing
   and Using the MySQL Preference Pane."

2.7.2. Installing the MySQL Startup Item

   The MySQL Installation Package includes a startup item that can be
   used to automatically startup and shutdown MySQL during boot.

   To install the MySQL Startup Item:

    1. Download and open the MySQL package installer, which is
       provided on a disk image (.dmg). Double-click to open the disk
       image, which includes the main MySQL installation package, the
       MySQLStartupItem.pkg installation package, and the
       MySQL.prefPane.

    2. Double-click on the MySQLStartItem.pkg file to start the
       installation process.

    3. You will be presented with the Install MySQL Startup Item
       dialog.
       MySQL Startup Item Installer: Step 1
       Click Continue to continue the installation process.

    4. A copy of the installation instructions and other important
       information relevant to this installation are display. Click
       Continue .

    5. Select the drive you want to use to install the MySQL Startup
       Item. The drive must have a valid, bootable, Mac OS X
       operating system installed. Click Continue.
       MySQL Startup Item Installer: Step 3

    6. You will be asked to confirm the details of the installation.
       To change the drive on which the startup item is installed you
       can click either Go Back or Change Install Location.... To
       install the startup item, click Install.

    7. Once the installation has been completed successfully, you
       will be given an Install Succeeded message.
       MySQL Startup Item Installer: Step 5

   The Startup Item for MySQL is installed into
   /Library/StartupItems/MySQLCOM. The Startup Item installation adds
   a variable MYSQLCOM=-YES- to the system configuration file
   /etc/hostconfig. If you want to disable the automatic startup of
   MySQL, simply change this variable to MYSQLCOM=-NO-.

   After the installation, you can start up MySQL by running the
   following commands in a terminal window. You must have
   administrator privileges to perform this task.

   If you have installed the Startup Item, use this command to start
   the server:
shell> sudo /Library/StartupItems/MySQLCOM/MySQLCOM start

   You may be prompted for your password to complete the startup.

   If you have installed the Startup Item, use this command to stop
   the server:
shell> sudo /Library/StartupItems/MySQLCOM/MySQLCOM stop

   You may be prompted for your password to complete the shutdown.

2.7.3. Installing and Using the MySQL Preference Pane

   The MySQL Package installer disk image also includes a custom
   MySQL Preference Pane that enables you to start, stop and control
   automated startup during boot of your MySQL installation.

   To install the MySQL Preference Pane:

    1. Download and open the MySQL package installer package, which
       is provided on a disk image (.dmg). Double-click to open the
       disk image, which includes the main MySQL installation
       package, the MySQLStartupItem.pkg installation package, and
       the MySQL.prefPane.

    2. Double click on MySQL.prefPane. The MySQL System Preferences
       will open.

    3. If this is the first time you have installed the preference
       pane, you will be asked to confirm installation and whether
       you want to install the preference pane for all users, or only
       the current user. To install the preference pane for all users
       you will need administrator privileges. If necessary, you will
       be prompted for the username and password for a user with
       administrator privileges.

    4. If you already have the MySQL Preference Pane installed, you
       will be asked to confirm whether you want to overwrite the
       existing MySQL Preference Pane.

Note

   The MySQL Preference Pane only starts and stops MySQL installation
   installed from the MySQL package installation that have been
   installed in the default location.

   Once the MySQL Preference Pane has been installed, you can control
   your MySQL server instance using the preference pane. To use the
   preference pane, open the System Preferences... from the Apple
   menu. Select the MySQL preference pane by clicking on the MySQL
   logo within the Other section of the preference panes list.
   MySQL Preference Pane

   The MySQL Preference Pane shows the current status of the MySQL
   server, showing stopped (in red) if the server is not running and
   running (in green) if the server has already been started. The
   preference pane will also show the current setting for whether the
   MySQL server has been set to start up automatically.

     * To start MySQL using the preference pane: 
       Click Start MySQL Server. You may be prompted for the username
       and password of a user with administrator privileges to start
       the MySQL server.

     * To stop MySQL using the preference pane: 
       Click Stop MySQL Server. You may be prompted for the username
       and password of a user with administrator privileges to
       shutdown the MySQL server.

     * To automatically start the MySQL server when the system boots:
       Check the checkbox next to Automatically Start MySQL Server on
       Startup.

     * To disable the automatic starting of the MySQL server when the
       system boots:
       Uncheck the checkbox next to Automatically Start MySQL Server
       on Startup.

   You can close the System Preferences... once you have completed
   your settings.

2.7.4. Using MySQL on Mac OS X Server

   If you are running Mac OS X Server, a version of MySQL should
   already be installed. The following table shows the versions of
   MySQL that ship with Mac OS X Server versions.
   Mac OS X Server Version MySQL Version
   10.2-10.2.2             3.23.51
   10.2.3-10.2.6           3.23.53
   10.3                    4.0.14
   10.3.2                  4.0.16
   10.4.0                  4.1.10a
   10.5.0                  5.0.45
   10.6.0                  5.0.82

   The installation layout of MySQL on Mac OS X Server is as shown in
   the table below:
   Directory                   Contents of Directory
   /usr/bin                    Client programs
   /var/mysql                  Log files, databases
   /usr/libexec                The mysqld server
   /usr/share/man              Unix manual pages
   /usr/share/mysql/mysql-test MySQL test suite
   /usr/share/mysql            Contains the mysql_install_db script
   /var/mysql/mysql.sock       The location of the MySQL Unix socket

Note

   The MySQL server bundled with Mac OS X Server does not include the
   MySQL client libraries and header files required if you want to
   access and use MySQL from a third-party driver, such as Perl DBI
   or PHP. For more information on obtaining and installing MySQL
   libraries, see Mac OS X Server version 10.5: MySQL libraries
   available for download (http://support.apple.com/kb/TA25017).
   Alternatively, you can ignore the bundled MySQL server and install
   MySQL from the package or tarball installation.

   For more information on managing the bundled MySQL instance in Mac
   OS X Server 10.5, see Mac OS X Server: Web Technologies
   Administration For Version 10.5 Leopard
   (http://images.apple.com/server/macosx/docs/Web_Technologies_Admin
   _v10.5.pdf). For more information on managing the bundled MySQL
   instance in Mac OS X Server 10.6, see Mac OS X Server: Web
   Technologies Administration Version 10.6 Snow Leopard
   (http://manuals.info.apple.com/en_US/WebTech_v10.6.pdf).

2.7.5. MySQL Installation on Mac OS X Notes

   You should keep the following issues and notes in mind:

     * The default location for the MySQL Unix socket is different on
       Mac OS X and Mac OS X Server depending on the installation
       type you chose. The default locations by installation are as
       follows:

       Package Installer from MySQL       /tmp/mysql.sock
       Tarball from MySQL                 /tmp/mysql.sock
       MySQL Bundled with Mac OS X Server /var/mysql/mysql.sock
       To prevent issues, you should either change the configuration
       of the socket used within your application (for example,
       changing php.ini), or you should configure the socket location
       using a MySQL configuration file and the socket option. For
       more information, see Section 5.1.2, "Server Command Options."

     * You may need (or want) to create a specific mysql user to own
       the MySQL directory and data. On Mac OS X 10.4 and lower you
       can do this by using the Netinfo Manager application, located
       within the Utilities folder within the Applications folder. On
       Mac OS X 10.5 and later you can do this through the Directory
       Utility. From Mac OS X 10.5 and later (including Mac OS X
       Server 10.5) the mysql should already exist. For use in single
       user mode, an entry for _mysql (note the underscore prefix)
       should already exist within the system /etc/passwd file.

     * Due to a bug in the Mac OS X package installer, you may see
       this error message in the destination disk selection dialog:
You cannot install this software on this disk. (null)
       If this error occurs, simply click the Go Back button once to
       return to the previous screen. Then click Continue to advance
       to the destination disk selection again, and you should be
       able to choose the destination disk correctly. We have
       reported this bug to Apple and it is investigating this
       problem.

     * Because the MySQL package installer installs the MySQL
       contents into a version and platform specific directory, you
       can use this to upgrade and migrate your database between
       versions. You will need to either copy the data directory from
       the old version to the new version, or alternatively specify
       an alternative datadir value to set location of the data
       directory.

     * You might want to add aliases to your shell's resource file to
       make it easier to access commonly used programs such as mysql
       and mysqladmin from the command line. The syntax for bash is:
alias mysql=/usr/local/mysql/bin/mysql
alias mysqladmin=/usr/local/mysql/bin/mysqladmin
       For tcsh, use:
alias mysql /usr/local/mysql/bin/mysql
alias mysqladmin /usr/local/mysql/bin/mysqladmin
       Even better, add /usr/local/mysql/bin to your PATH environment
       variable. You can do this by modifying the appropriate startup
       file for your shell. For more information, see Section 4.2.1,
       "Invoking MySQL Programs."

     * After you have copied over the MySQL database files from the
       previous installation and have successfully started the new
       server, you should consider removing the old installation
       files to save disk space. Additionally, you should also remove
       older versions of the Package Receipt directories located in
       /Library/Receipts/mysql-VERSION.pkg.
