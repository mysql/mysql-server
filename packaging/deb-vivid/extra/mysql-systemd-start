#!/bin/bash

# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# Scripts to run by MySQL systemd service
#
# Needed argument: pre | post
#
# pre mode  :  try to perform sanity check for configuration, log, data
# post mode :  ping server until answer is received

pinger () {
	while /bin/true ; do
		sleep 1
		mysqladmin ping >/dev/null 2>&1 && break
	done
}

sanity () {
	MYSQLRUN=/var/run/mysqld
	MYSQLDATA=/var/lib/mysql
	MYSQLLOG=/var/log/mysql

	if [ ! -d ${MYSQLDATA} -a ! -L ${MYSQLDATA} ];
	then
		mkdir ${MYSQLDATA}
		chown mysql:mysql ${MYSQLDATA}
		chmod 750 ${MYSQLDATA}
	fi

	if [ ! -d "${MYSQLDATA}/mysql" -a ! -L "${MYSQLDATA}/mysql" ];
	then
		mkdir ${MYSQLDATA}/mysql
		chown mysql:mysql ${MYSQLDATA}/mysql
		chmod 750 ${MYSQLDATA}/mysql
	fi

	if [ ! "$(ls -A ${MYSQLDATA}/mysql)" ];
	then
		mysql_install_db --user=mysql > /dev/null
	fi

	if [ ! -d ${MYSQLLOG} -a ! -L ${MYSQLLOG} ];
	then
		mkdir ${MYSQLLOG}
		chown mysql:adm ${MYSQLLOG}
		chmod 750 ${MYSQLLOG}
		touch ${MYSQLLOG}/error.log
		chmod 640 ${MYSQLLOG}/error.log
		chown mysql:adm ${MYSQLLOG}/error.log
	fi

	if [ ! -d "${MYSQLRUN}" -a ! -L "${MYSQLRUN}" ];
	then
		mkdir ${MYSQLRUN}
		chown mysql:mysql ${MYSQLRUN}
		chmod 755 ${MYSQLRUN}
	fi

	/lib/init/apparmor-profile-load usr.sbin.mysqld

	if [ ! -r /etc/mysql/my.cnf ]; then
		echo "MySQL configuration not found at /etc/mysql/my.cnf. Please install one using update-alternatives."
		exit 1
	fi
}

case $1 in
	"pre")  sanity ;;
	"post") pinger ;;
esac
