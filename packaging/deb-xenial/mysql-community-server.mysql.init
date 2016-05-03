#!/bin/bash
#
### BEGIN INIT INFO
# Provides:          mysql
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Should-Start:      $network $time
# Should-Stop:       $network $time
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start/ Stop MySQL Community Server daemon
# Description:       This service script facilitates startup and shutdown of
#                    mysqld daemon throught its wrapper script mysqld_safe
### END INIT INFO
#

# Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

. /lib/lsb/init-functions

cd /
umask 077

MYSQLDATA=/var/lib/mysql
VERSION=$(mysqld --version | grep mysqld | cut -d' ' -f4)

get_mysql_option() {
	RESULT=$(my_print_defaults "$1" | sed -n "s/^--$2=//p" | tail -n 1)
	if [ -z "$RESULT" ];
	then
		RESULT="$3"
	fi
	echo $RESULT
}

get_running () {
	PIDFILE=$(get_mysql_option mysqld_safe pid-file "")
	if [ -z "$PIDFILE" ];
	then
		PIDFILE=$(get_mysql_option mysqld pid-file "$MYSQLDATA/$(hostname).pid")
	fi
	if [ -e "$PIDFILE" ] && [ -d "/proc/$(cat "$PIDFILE")" ];
	then
		echo 1
	else
		echo 0
	fi
}

server_stop () {
	RUNNING=$(get_running)
	COUNT=0
	while :; do
		COUNT=$(( COUNT+1 ))
		echo -n .
		if [ "${RUNNING}" -eq 0 ];
		then
			echo
			break
		fi
		if [ "${COUNT}" -gt 15 ];
		then
			echo
			return 1
		fi
		RUNNING=$(get_running)
		sleep 1
	done
	return 0
}

case "$1" in
  'start')
	RUNNING=$(get_running)
	if [ "${RUNNING}" -eq 1 ];
	then
		log_action_msg "A MySQL Server is already started"
	else
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

		su - mysql -s /bin/bash -c "mysqld_safe > /dev/null &"
		for i in 1 2 3 4 5 6;
		do
			sleep 1
			echo -n .
		done
		echo
		RUNNING=$(get_running)
		if [ "${RUNNING}" -eq 1 ];
		then
			log_action_msg "MySQL Community Server ${VERSION} is started"
		else
			log_action_msg "MySQL Community Server ${VERSION} did not start. Please check logs for more details."
		fi
	fi
	;;

  'stop')
	RUNNING=$(get_running)
	if [ "${RUNNING}" -eq 1 ];
	then
		killall -15 mysqld
		server_stop
		if [ "$?" -eq 0 ];
		then
			log_action_msg "MySQL Community Server ${VERSION} is stopped"
		else
			log_action_msg "Attempt to shutdown MySQL Community Server ${VERSION} timed out"
		fi
	else
		log_action_msg "MySQL Community Server ${VERSION} is already stopped"
	fi
	;;

  'restart'|'reload'|'force-reload')
	log_action_msg "Stopping MySQL Community Server ${VERSION}"
	$0 stop
	log_action_msg "Re-starting MySQL Community Server ${VERSION}"
	$0 start
	;;

  'status')
	RUNNING=$(get_running)
	if [ ${RUNNING} -eq 1 ];
	then
		log_action_msg "MySQL Community Server ${VERSION} is running"
	else
		log_action_msg "MySQL Community Server ${VERSION} is not running"
		exit 3
	fi
	;;

  *)
	echo "Usage: $SELF start|stop|restart|reload|force-reload|status"
	exit 1
	;;
esac

exit 0
