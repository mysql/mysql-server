# The log file name and location can be set in
# /etc/my.cnf by setting the "log-error" option
# in either [mysqld] or [mysqld_safe] section as
# follows:
#
# [mysqld]
# log-error=@localstatedir@/mysqld.log
#
# In case the root user has a password, then you
# have to create a /root/.my.cnf configuration file
# with the following content:
#
# [mysqladmin]
# password = <secret> 
# user= root
#
# where "<secret>" is the password. 
#
# ATTENTION: The /root/.my.cnf file should be readable
# _ONLY_ by root !

@localstatedir@/mysqld.log {
        # create 600 mysql mysql
        notifempty
        daily
        rotate 5
        missingok
        compress
    postrotate
	# just if mysqld is really running
	if test -x @bindir@/mysqladmin && \
	   @bindir@/mysqladmin ping &>/dev/null
	then
	   @bindir@/mysqladmin flush-logs
	fi
    endscript
}
