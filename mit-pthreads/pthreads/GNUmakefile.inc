#	from: @(#)Makefile.inc	5.6 (Berkeley) 6/4/91

# pthread sources
VPATH := $(VPATH):${srcdir}/pthreads

SRCS:= cleanup.c cond.c fd.c fd_kern.c fd_pipe.c fd_sysv.c file.c globals.c \
       malloc.c mutex.c pthread.c pthread_attr.c queue.c signal.c machdep.c \
       syscall.S pthread_join.c pthread_detach.c pthread_once.c sleep.c \
       specific.c process.c wait.c errno.c schedparam.c _exit.c prio_queue.c \
       pthread_init.c init.cc sig.c info.c mutexattr.c select.c wrapper.c \
       dump_state.c pthread_kill.c stat.c readv.c writev.c condattr.c \
       pthread_cancel.c panic.c $(SRCS)

ifeq ($(HAVE_SYSCALL_TEMPLATE),yes)
SYSCALL_FILTER_RULE=	for s in $(AVAILABLE_SYSCALLS) ; do \
	  case " $(SYSCALL_EXCEPTIONS) " in \
	    *" "$$s" "*) ;; \
	    *)	echo $$s ;; \
	  esac ; \
	done
STD_SYSCALLS:=$(shell $(SYSCALL_FILTER_RULE))
STD_SYSCALL_FILES:= $(addprefix S,$(addsuffix .o,$(STD_SYSCALLS)))
EXTRA_OBJS := $(EXTRA_OBJS) syscalls.o
# EXTRA_OBJS := $(EXTRA_OBJS) $(STD_SYSCALL_FILES)

ifndef SYSCALL_PIC_COMPILE
SYSCALL_PIC_COMPILE=true
endif

obj/syscalls.o: syscall-template.S
	-rm -rf obj/syscalls
	mkdir obj/syscalls
	for syscall in $(STD_SYSCALLS) ; do \
	  echo $$syscall ; \
	  $(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -c syscall-template.S -o obj/syscalls/S$$syscall.o ; \
	  $(SYSCALL_PIC_COMPILE) ; \
	done
	x=`pwd` && cd obj/syscalls && ld -r -o ../syscalls.o S*.o && cd $$x
	rm -r obj/syscalls
endif

syscall.o: ${.CURDIR}/pthreads/syscall.S
	cpp ${CPPFLAGS} ${.CURDIR}/pthreads/syscall.S > syscall.i
	as syscall.i 
	rm syscall.i
	mv a.out syscall.o
