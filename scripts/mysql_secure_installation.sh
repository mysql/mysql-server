#!/bin/sh

# Copyright (C) 2002 MySQL AB and Jeremy Cole
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

config=".my.cnf.$$"
command=".mysql.$$"

trap "interrupt" 2

rootpass=""

prepare() {
    touch $config $command
    chmod 600 $config $command
}

do_query() {
    echo $1 >$command
    mysql --defaults-file=$config <$command
    return $?
}

make_config() {
    echo "# mysql_secure_installation config file" >$config
    echo "[mysql]" >>$config
    echo "user=root" >>$config
    echo "password=$rootpass" >>$config
}

get_root_password() {
    status=1
    while [ $status -eq 1 ]; do
	stty -echo
	echo -n "Enter current password for root (enter for none): "
	read password
	echo
	stty echo
	if [ "x$password" = "x" ]; then
	    hadpass=0
	else
	    hadpass=1
	fi
	rootpass=$password
	make_config
	do_query ""
	status=$?
    done
    echo "OK, successfully used password, moving on..."
    echo
}

set_root_password() {
    stty -echo
    echo -n "New password: "
    read password1
    echo
    echo -n "Re-enter new password: "
    read password2
    echo
    stty echo

    if [ "$password1" != "$password2" ]; then
	echo "Sorry, passwords do not match."
	echo
	return 1
    fi

    if [ "$password1" = "" ]; then
	echo "Sorry, you can't use an empty password here."
	echo
	return 1
    fi

    do_query "UPDATE mysql.user SET Password=PASSWORD('$password1') WHERE User='root';"
    if [ $? -eq 0 ]; then
	echo "Password updated successfully!"
	echo "Reloading privilege tables.."
	if ! reload_privilege_tables; then
	    exit 1
	fi
	echo
	rootpass=$password1
	make_config
    else
	echo "Password update failed!"
	exit 1
    fi

    return 0
}

remove_anonymous_users() {
    do_query "DELETE FROM mysql.user WHERE User='';"
    if [ $? -eq 0 ]; then
	echo " ... Success!"
    else
	echo " ... Failed!"
	exit 1
    fi

    return 0
}

remove_remote_root() {
    do_query "DELETE FROM mysql.user WHERE User='root' AND Host!='localhost';"
    if [ $? -eq 0 ]; then
	echo " ... Success!"
    else
	echo " ... Failed!"
    fi
}

remove_test_database() {
    echo " - Dropping test database..."
    do_query "DROP DATABASE test;"
    if [ $? -eq 0 ]; then
	echo " ... Success!"
    else
	echo " ... Failed!  Not critical, keep moving..."
    fi

    echo " - Removing privileges on test database..."
    do_query "DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%'"
    if [ $? -eq 0 ]; then
	echo " ... Success!"
    else
	echo " ... Failed!  Not critical, keep moving..."
    fi

    return 0
}

reload_privilege_tables() {
    do_query "FLUSH PRIVILEGES;"
    if [ $? -eq 0 ]; then
	echo " ... Success!"
	return 0
    else
	echo " ... Failed!"
	return 1
    fi
}

interrupt() {
    echo
    echo "Aborting!"
    echo
    cleanup
    stty echo
    exit 1
}

cleanup() {
    echo "Cleaning up..."
    rm -f $config $command
}


# The actual script starts here

prepare

echo
echo
echo
echo
echo "NOTE: RUNNING ALL PARTS OF THIS SCRIPT IS RECOMMENDED FOR ALL MySQL"
echo "      SERVERS IN PRODUCTION USE!  PLEASE READ EACH STEP CAREFULLY!"
echo
echo

echo "In order to log into MySQL to secure it, we'll need the current"
echo "password for the root user.  If you've just installed MySQL, and"
echo "you haven't set the root password yet, the password will be blank,"
echo "so you should just press enter here."
echo

get_root_password


#
# Set the root password
#

echo "Setting the root password ensures that nobody can log into the MySQL"
echo "root user without the proper authorisation."
echo

if [ $hadpass -eq 0 ]; then
    echo -n "Set root password? [Y/n] "
else
    echo "You already have a root password set, so you can safely answer 'n'."
    echo
    echo -n "Change the root password? [Y/n] "
fi

read reply
if [ "$reply" = "n" ]; then
    echo " ... skipping."
else
    status=1
    while [ $status -eq 1 ]; do
	set_root_password
	status=$?
    done
fi
echo


#
# Remove anonymous users
#

echo "By default, a MySQL installation has an anonymous user, allowing anyone"
echo "to log into MySQL without having to have a user account created for"
echo "them.  This is intended only for testing, and to make the installation"
echo "go a bit smoother.  You should remove them before moving into a"
echo "production environment."
echo

echo -n "Remove anonymous users? [Y/n] "

read reply
if [ "$reply" = "n" ]; then
    echo " ... skipping."
else
    remove_anonymous_users
fi
echo


#
# Disallow remote root login
#

echo "Normally, root should only be allowed to connect from 'localhost'.  This"
echo "ensures that someone cannot guess at the root password from the network."
echo

echo -n "Disallow root login remotely? [Y/n] "
read reply
if [ "$reply" = "n" ]; then
    echo " ... skipping."
else
    remove_remote_root
fi
echo


#
# Remove test database
#

echo "By default, MySQL comes with a database named 'test' that anyone can"
echo "access.  This is also intended only for testing, and should be removed"
echo "before moving into a production environment."
echo

echo -n "Remove test database and access to it? [Y/n] "
read reply
if [ "$reply" = "n" ]; then
    echo " ... skipping."
else
    remove_test_database
fi
echo


#
# Reload privilege tables
#

echo "Reloading the privilege tables will ensure that all changes made so far"
echo "will take effect immediately."
echo

echo -n "Reload privilege tables now? [Y/n] "
read reply
if [ "$reply" = "n" ]; then
    echo " ... skipping."
else
    reload_privilege_tables
fi
echo

cleanup

echo
echo
echo
echo "All done!  If you've completed all of the above steps, your MySQL"
echo "installation should now be secure."
echo
echo "Thanks for using MySQL!"
echo
echo


