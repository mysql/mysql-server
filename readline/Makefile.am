## Process this file with automake to create Makefile.in
# Makefile for the GNU readline library.
# Copyright (C) 1994,1996,1997 Free Software Foundation, Inc.

INCLUDES =		-I$(srcdir)/../include -I$(srcdir)/.. -I..

noinst_LIBRARIES =	libreadline.a

libreadline_a_SOURCES =	readline.c funmap.c keymaps.c \
			vi_mode.c parens.c rltty.c \
			complete.c bind.c isearch.c \
			display.c signals.c \
			util.c kill.c \
			undo.c macro.c input.c \
			callback.c terminal.c xmalloc.c \
			history.c histsearch.c histexpand.c \
			histfile.c nls.c search.c \
			shell.c tilde.c 

pkginclude_HEADERS =	readline.h chardefs.h keymaps.h history.h tilde.h

noinst_HEADERS =	rldefs.h histlib.h rlwinsize.h \
			posixstat.h posixdir.h posixjmp.h \
			tilde.h rlconf.h rltty.h ansi_stdlib.h \
			tcap.h rlstdc.h

EXTRA_DIST=		emacs_keymap.c vi_keymap.c

DEFS =			-DUNDEF_THREADS_HACK -DHAVE_CONFIG_H

# Don't update the files from bitkeeper
%::SCCS/s.%
