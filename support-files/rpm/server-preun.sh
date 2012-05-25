if [ $1 = 0 ] ; then
	# Stop MySQL before uninstalling it
	if [ -x %{_sysconfdir}/init.d/mysql ] ; then
		%{_sysconfdir}/init.d/mysql stop > /dev/null
	fi
        # Don't start it automatically anymore
	if [ -x /sbin/chkconfig ] ; then
		/sbin/chkconfig --del mysql
	fi
fi

# We do not remove the mysql user since it may still own a lot of
# database files.

