##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

DIST_SUBDIRS = lib src po tests debug
SUBDIRS =

if COND_GNULIB
SUBDIRS += lib
endif

SUBDIRS += src po tests

dist_doc_DATA = \
	AUTHORS \
	COPYING \
	COPYING.GPLv2 \
	NEWS \
	README \
	THANKS \
	TODO \
	doc/faq.txt \
	doc/history.txt \
	doc/xz-file-format.txt \
	doc/lzma-file-format.txt

EXTRA_DIST = \
	extra \
	dos \
	windows \
	autogen.sh \
	version.sh \
	Doxyfile.in \
	COPYING.GPLv2 \
	COPYING.GPLv3 \
	COPYING.LGPLv2.1 \
	INSTALL.generic \
	PACKAGERS

ACLOCAL_AMFLAGS = -I m4

# List of man pages to conver to PDF and plain text in the dist-hook target.
manfiles = \
	src/xz/xz.1 \
	src/xzdec/xzdec.1 \
	src/scripts/xzdiff.1 \
	src/scripts/xzgrep.1 \
	src/scripts/xzless.1 \
	src/scripts/xzmore.1

# Create ChangeLog from output of "git log --date=iso --stat".
# Convert the man pages to PDF and plain text (ASCII only) formats.
dist-hook:
	if test -d "$(srcdir)/.git" && type git > /dev/null 2>&1; then \
		( cd "$(srcdir)" && git log --date=iso --stat ) \
			> "$(distdir)/ChangeLog"; \
	fi
	if type groff > /dev/null 2>&1 && type ps2pdf > /dev/null 2>&1; then \
		dest="$(distdir)/doc/man" && \
		$(MKDIR_P) "$$dest/pdf-a4" "$$dest/pdf-letter" "$$dest/txt" && \
		for FILE in $(manfiles); do \
			BASE=`basename $$FILE .1` && \
			groff -man -t -Tps -P-pa4 < "$(srcdir)/$$FILE" \
				| ps2pdf - - \
				> "$$dest/pdf-a4/$$BASE-a4.pdf" && \
			groff -man -t -Tps -P-pletter < "$(srcdir)/$$FILE" \
				| ps2pdf - - \
				> "$$dest/pdf-letter/$$BASE-letter.pdf" && \
			groff -man -t -Tascii < "$(srcdir)/$$FILE" \
				| col -bx > "$$dest/txt/$$BASE.txt"; \
		done; \
	fi

# This works with GNU tar and gives cleaner package than normal 'make dist'.
mydist:
	TAR_OPTIONS='--owner=0 --group=0 --numeric-owner --mode=u+rw,go+r-w' \
		$(MAKE) dist-gzip
