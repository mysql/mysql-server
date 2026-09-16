dist_sysconf_DATA	= netconfig bindresvport.blacklist 

CLEANFILES	       = cscope.* *~
DISTCLEANFILES	       = Makefile.in

if ! INET6
install-exec-hook:
	$(SED) -i '/^tcp6\|^udp6/d' "$(DESTDIR)$(sysconfdir)"/netconfig
endif
