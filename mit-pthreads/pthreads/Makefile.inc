#	from: @(#)Makefile.inc	5.6 (Berkeley) 6/4/91

# pthread sources
.PATH: ${srcdir}/pthreads

SRCS+= cleanup.c cond.c fd.c fd_kern.c fd_pipe.c file.c globals.c malloc.c \
       mutex.c pthread.c pthread_attr.c queue.c signal.c machdep.c syscall.S \
       pthread_join.c pthread_detach.c pthread_once.c sleep.c specific.c \
       process.c wait.c errno.c schedparam.c _exit.c prio_queue.c \
       pthread_init.c init.cc sig.c info.c mutexattr.c select.c wrapper.c \
       dump_state.c pthread_kill.c condattr.c pthread_cancel.c panic.c

.if $(HAVE_SYSCALL_TEMPLATE) == yes
OBJS+= syscalls.o
.if !defined(NOPIC)
SOBJS+= syscalls.so
SYSCALL_PIC_COMPILE= $(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -DPIC -c ${.CURDIR}/syscall-template.S -o ${.OBJDIR}/syscalls/S$$syscall.so
.else
SYSCALL_PIC_COMPILE= true
.endif
.if !defined(NOPROFILE)
POBJS+= syscalls.po
SYSCALL_PROF_COMPILE= $(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -pg -c ${.CURDIR}/syscall-template.S -o ${.OBJDIR}/syscalls/S$$syscall.po
.else
SYSCALL_PROF_COMPILE= true
.endif

OPSYS!=	uname -s

syscalls.o syscalls.so syscalls.po : syscall-template.S
	-rm -rf ${.OBJDIR}/syscalls
	mkdir ${.OBJDIR}/syscalls
	for syscall in $(AVAILABLE_SYSCALLS) ; do \
	  case " $(SYSCALL_EXCEPTIONS) " in \
	    *" "$$syscall" "*) ;; \
	    *)	echo $$syscall ; \
		$(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -c ${.CURDIR}/syscall-template.S -o ${.OBJDIR}/syscalls/S$$syscall.o ; \
		$(SYSCALL_PIC_COMPILE) ; \
		$(SYSCALL_PROF_COMPILE) ;; \
	  esac ; \
	done
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.o *.o && cd $$x
.if !defined(NOPIC)
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.so *.so && cd $$x
.endif
.if !defined(NOPROFILE)
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.po *.po && cd $$x
.endif
	rm -r ${.OBJDIR}/syscalls
.endif

syscall.o: syscall.S
.if (${OPSYS} == "FreeBSD")
	$(CC) -c -x assembler-with-cpp -o syscall.o ${.CURDIR}/syscall.S
.else
	cpp ${CPPFLAGS} ${.CURDIR}/syscall.S > syscall.i
	as syscall.i 
	rm syscall.i
	mv a.out syscall.o
.endif

syscall.po: syscall.S
.if (${OPSYS} == "FreeBSD")
	$(CC) -c -x assembler-with-cpp -o syscall.po ${.CURDIR}/syscall.S
.else
	cpp ${CPPFLAGS} ${.CURDIR}/syscall.S > syscall.i
	as syscall.i 
	rm syscall.i
	mv a.out syscall.po
.endif

MAN2+=

MAN3+=

