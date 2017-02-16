##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

bin_PROGRAMS = xz

xz_SOURCES = \
	args.c \
	args.h \
	coder.c \
	coder.h \
	file_io.c \
	file_io.h \
	hardware.c \
	hardware.h \
	main.c \
	main.h \
	message.c \
	message.h \
	options.c \
	options.h \
	private.h \
	signals.c \
	signals.h \
	suffix.c \
	suffix.h \
	util.c \
	util.h

if COND_W32
xz_SOURCES += xz_w32res.rc
endif

xz_CPPFLAGS = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_builddir)/lib \
	$(STATIC_CPPFLAGS)

xz_LDFLAGS = $(STATIC_LDFLAGS)
xz_LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
xz_LDADD += $(top_builddir)/lib/libgnu.a
endif

# libgnu.a may need these libs, so this must be after libgnu.a.
xz_LDADD += $(LTLIBINTL)


# Windows resource compiler support
.rc.o:
	$(RC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(xz_CPPFLAGS) $(CPPFLAGS) $(RCFLAGS) -i $< -o $@


dist_man_MANS = xz.1


## Create symlinks for unxz and xzcat for convenience. Create symlinks also
## for lzma, unlzma, and lzcat for compatibility with LZMA Utils 4.32.x.
xzlinks = unxz xzcat lzma unlzma lzcat

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	target=`echo xz | sed '$(transform)'`$(EXEEXT) && \
	for name in $(xzlinks); do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	target=`echo xz | sed '$(transform)'` && \
	for name in $(xzlinks); do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	for name in $(xzlinks); do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link; \
	done
	cd $(DESTDIR)$(mandir)/man1 && \
	for name in $(xzlinks); do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link.1; \
	done
