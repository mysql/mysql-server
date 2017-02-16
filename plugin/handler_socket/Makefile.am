
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = @HANDLERSOCKET_SUBDIRS@
EXTRA_DIST= plug.in

perl:
	cd perl-Net-HandlerSocket && perl Makefile.PL && make

install_perl:
	cd perl-Net-HandlerSocket && make install

rpms: rpm_cli rpm_perl rpm_c

rpm_dir:
	- mkdir dist
	- mkdir dist/BUILD dist/RPMS dist/SOURCES dist/SPECS dist/SRPMS

rpm_cli: clean_cli rpm_dir
	sed -e "s/HANDLERSOCKET_VERSION/$(VERSION)/" \
		libhsclient/libhsclient.spec.template \
		> libhsclient/libhsclient.spec
	tar cvfz dist/libhsclient.tar.gz libhsclient
	rpmbuild --define "_topdir `pwd`/dist" -ta \
		dist/libhsclient.tar.gz

rpm_perl: clean_perl rpm_dir
	sed -e "s/HANDLERSOCKET_VERSION/$(VERSION)/" \
		perl-Net-HandlerSocket/perl-Net-HandlerSocket.spec.template \
		> perl-Net-HandlerSocket/perl-Net-HandlerSocket.spec 
	cd perl-Net-HandlerSocket && perl Makefile.PL && make clean && \
		rm -f Makefile.old
	tar cvfz dist/perl-Net-HandlerSocket.tar.gz perl-Net-HandlerSocket
	rpmbuild --define "_topdir `pwd`/dist" -ta \
		dist/perl-Net-HandlerSocket.tar.gz

rpm_c: clean_c rpm_dir
	sed -e "s/HANDLERSOCKET_VERSION/$(VERSION)/" \
		handlersocket/handlersocket.spec.template \
		> handlersocket/handlersocket.spec
	sed -e "s|HANDLERSOCKET_MYSQL_INC|$(MYSQL_CFLAGS) $(MYSQL_INC)|" \
		-e "s|HANDLERSOCKET_MYSQL_LIB|$(MYSQL_LIB)|" \
		handlersocket/Makefile.plain.template \
		> handlersocket/Makefile.plain
	tar cvfz dist/handlersocket.tar.gz handlersocket
	rpmbuild --define "_topdir `pwd`/dist" -ta \
		dist/handlersocket.tar.gz

install_rpm_pl:
	- sudo rpm -e perl-Net-HandlerSocket
	- sudo rpm -e perl-Net-HandlerSocket-debuginfo
	make clean
	make rpm_perl
	- sudo rpm -U dist/RPMS/*/perl*.rpm

installrpms:
	- sudo rpm -e handlersocket
	- sudo rpm -e handlersocket-debuginfo
	- sudo rpm -e perl-Net-HandlerSocket
	- sudo rpm -e perl-Net-HandlerSocket-debuginfo
	- sudo rpm -e libhsclient
	- sudo rpm -e libhsclient-debuginfo
	make clean
	make rpm_cli
	- sudo rpm -U dist/RPMS/*/libhsclient*.rpm
	make clean
	make rpm_perl
	- sudo rpm -U dist/RPMS/*/perl*.rpm
	make clean
	make rpm_c
	- sudo rpm -U dist/RPMS/*/handlersocket*.rpm

clean_cli:
	cd libhsclient && make clean
	cd client && make clean

clean_perl:
	cd perl-Net-HandlerSocket && perl Makefile.PL && make clean && \
                rm -f Makefile.old

clean_c:
	cd handlersocket && make clean

clean_all: clean_cli clean_perl clean_c
	cd regtest && make clean
	rm -rf dist/*/*
	rm -f dist/*.tar.gz

