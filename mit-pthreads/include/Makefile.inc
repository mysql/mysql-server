#	from: @(#)Makefile	5.45.1.1 (Berkeley) 5/6/91

# Doing a make install builds /usr/include/pthread
#
# The ``rm -rf''s used below are safe because rm doesn't follow symbolic
# links.


FILES= cond.h copyright.h fd.h fd_pipe.h kernel.h mutex.h posix.h \
	   pthread.h pthread_attr.h queue.h util.h 

# Machine dependent header file
MFILE= ${.CURDIR}/arch/${MACHINE}/machdep.h

realinstall:
	if [ ! -d ${DESTDIR}/usr/include/pthread ]; then \
		mkdir ${DESTDIR}/usr/include/pthread; \
	fi 
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/pthread/$$i || \
		    install -c -m 644 $$i ${DESTDIR}/usr/include/$$i; \
	done
	cmp -s ${MFILE} ${DESTDIR}/usr/include/pthread/machdep.h || \
	  install -c -m 644 ${MFILE} ${DESTDIR}/usr/include/pthread/machdep.h
	rm -rf ${DESTDIR}/usr/include/pthread.h
	ln -s /usr/include/pthread/pthread.h ${DESTDIR}/usr/include/pthread.h
	@chown -R ${BINOWN}:${BINGRP} ${DESTDIR}/usr/include/pthread
	@chmod -R a-w ${DESTDIR}/usr/include/pthread

