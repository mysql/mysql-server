##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

# Windows resource compiler support. It's fine to use xz_CPPFLAGS
# also for lzmadec.
.rc.o:
	$(RC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
		$(xzdec_CPPFLAGS) $(CPPFLAGS) $(RCFLAGS) -i $< -o $@


bin_PROGRAMS = xzdec lzmadec

xzdec_SOURCES = xzdec.c

if COND_W32
xzdec_SOURCES += xzdec_w32res.rc
endif

xzdec_CPPFLAGS = \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api \
	-I$(top_builddir)/lib \
	$(STATIC_CPPFLAGS)
xzdec_LDFLAGS = $(STATIC_LDFLAGS)
xzdec_LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
xzdec_LDADD += $(top_builddir)/lib/libgnu.a
endif

xzdec_LDADD += $(LTLIBINTL)


lzmadec_SOURCES = xzdec.c

if COND_W32
lzmadec_SOURCES += lzmadec_w32res.rc
endif

lzmadec_CPPFLAGS = $(xzdec_CPPFLAGS) -DLZMADEC
lzmadec_LDFLAGS = $(xzdec_LDFLAGS)
lzmadec_LDADD = $(xzdec_LDADD)


dist_man_MANS = xzdec.1

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	target=`echo xzdec | sed '$(transform)'` && \
	link=`echo lzmadec | sed '$(transform)'` && \
	rm -f $$link.1 && \
	$(LN_S) $$target.1 $$link.1

uninstall-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	link=`echo lzmadec | sed '$(transform)'` && \
	rm -f $$link.1
