# This logname is set in mysql.server.sh that ends up in /etc/rc.d/init.d/mysql
#
# If the root user has a password you have to create a
# /root/.my.cnf configuration file with the following
# content:
#
# [mysqladmin]
# password = <secret> 
# user= root
#
# where "<secret>" is the password. 
#
# ATTENTION: This /root/.my.cnf should be readable ONLY
# for root !

@localstatedir@/mysqld.log {
        # create 600 mysql mysql
        notifempty
	daily
        rotate 3
        missingok
        compress
    postrotate
	# just if mysqld is really running
	if test -n "`ps acx|grep mysqld`"; then
	        @bindir@/mysqladmin flush-logs
	fi
    endscript
}
