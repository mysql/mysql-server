Installation notes for MySQL on Mac OS X

PLEASE READ!

For more details about installing and running
MySQL on Mac OS X, also refer to the manual,
which is available online:

http://www.mysql.com/doc/en/Mac_OS_X_installation.html

NOTE: Before proceeding with the installation, please
make sure that no other MySQL server is running!

Please shut down all running MySQL instances before
continuing by either using the MySQL Manager
Application (on Mac OS X Server) or via "mysqladmin
shutdown" on the command line.

This MySQL package will be installed into
"/usr/local/mysql-<version>" and will also create a
symbolic link "/usr/local/mysql", pointing to the new
location.

A previously existing /usr/local/mysql directory will
be renamed to /usr/local/mysql.bak before proceeding
with the installation.

Additionally, it will install the mysql grant tables by
executing "mysql_install_db" after the installation.

If you are running Mac OS X Server, you already have a
version MySQL installed. Make sure to read Apple's help
about installing MySQL (Run the "Help View" application,
select "Mac OS X Server help", and do a search for MySQL
and read the item entitled "Installing MySQL").

If you previously used Marc Liyanage's MySQL packages
for MacOS X from http://www.entropy.ch, you can simply
follow the update instructions given on his pages.

After the installation (and restoring the old database
files, if necessary), you can start up MySQL by running
the following commands in a terminal window:

  cd /usr/local/mysql
  sudo ./bin/mysqld_safe
  (Enter your password)
  (Press CTRL+Z)
  (Press CTRL+D to exit the shell)
  bg

You should now be able to connect to the MySQL server,
e.g. by running /usr/local/mysql/bin/mysql

If you installed MySQL for the first time,
PLEASE REMEMBER TO SET A PASSWORD FOR THE MySQL root USER!
This is done with the following two commands:

/usr/local/mysql/bin/mysqladmin -u root password 'new-password'

/usr/local/mysql/bin/mysqladmin -u root -h $hostname password 'new-password'

Please note, that after upgrading from MySQL 3.23 to
MySQL 4.0 it is recommended to convert the MySQL
privilege tables using the mysql_fix_privilege_tables
script, since some new security privileges have been
added.

Please see
http://www.mysql.com/doc/en/Upgrading-from-3.23.html
for more information on how to upgrade from MySQL 3.23.

If you do not want to have to type the full path
"/usr/local/mysql/bin" in front of every command, you
can to add this directory to your PATH environment
variable in your login script. For the default shell
"tcsh", you can do this by running this command once:

echo 'setenv PATH $PATH:/usr/local/mysql/bin' >> ~/.tcshrc

