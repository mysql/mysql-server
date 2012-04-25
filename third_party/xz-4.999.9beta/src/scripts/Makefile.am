##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

nodist_bin_SCRIPTS = xzdiff xzgrep xzmore xzless
dist_man_MANS = xzdiff.1 xzgrep.1 xzmore.1 xzless.1

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	target=`echo xzdiff | sed '$(transform)'` && \
	for name in xzcmp lzdiff lzcmp; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done && \
	target=`echo xzgrep | sed '$(transform)'` && \
	for name in xzegrep xzfgrep lzgrep lzegrep lzfgrep; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done && \
	for name in xzmore xzless; do \
		target=`echo $$name | sed '$(transform)'` && \
		link=`echo $$name | sed 's/xz/lz/;$(transform)'` && \
		rm -f $$link && \
		$(LN_S) $$target $$link; \
	done

install-data-hook:
	cd $(DESTDIR)$(mandir)/man1 && \
	target=`echo xzdiff | sed '$(transform)'` && \
	for name in xzcmp lzdiff lzcmp; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done && \
	target=`echo xzgrep | sed '$(transform)'` && \
	for name in xzegrep xzfgrep lzgrep lzegrep lzfgrep; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done && \
	for name in xzmore xzless; do \
		target=`echo $$name | sed '$(transform)'` && \
		link=`echo $$name | sed 's/xz/lz/;$(transform)'` && \
		rm -f $$link.1 && \
		$(LN_S) $$target.1 $$link.1; \
	done

uninstall-hook:
	cd $(DESTDIR)$(bindir) && \
	for name in xzcmp lzdiff lzcmp xzegrep xzfgrep \
			lzgrep lzegrep lzfgrep lzmore lzless; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link; \
	done
	cd $(DESTDIR)$(mandir)/man1 && \
	for name in xzcmp lzdiff lzcmp xzegrep xzfgrep \
			lzgrep lzegrep lzfgrep lzmore lzless; do \
		link=`echo $$name | sed '$(transform)'` && \
		rm -f $$link.1; \
	done
