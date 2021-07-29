AUTOMAKE_OPTIONS=gnu
ACLOCAL_AMFLAGS=-I tests/m4
SUBDIRS = src examples java tests contrib
bin_SCRIPTS = tests/scripts/fastbit-config
EXTRA_DIST = runconfig win/README win/Win.mak win/MinGW.mak win/ardea.vcproj \
	win/ibis.sln win/ibis.vcproj win/thula.vcproj win/tcapi.vcproj \
	win/trydll.cpp win/dll.vcproj win/trydll.vcproj win/java.vcproj \
	win/FlexLexer.h contrib/README contrib/fbmerge/fbmerge.cpp \
	contrib/fbmerge/Makefile.am contrib/fbmerge/Makefile.in \
	contrib/fbmerge/fbmerge.sln contrib/fbmerge/fbmerge.vcproj

dist_doc_DATA = doc/dataDistributions.html \
	doc/index.html doc/quickstart.html doc/dataLoading.html \
	doc/ibisCommandLine.html doc/indexSpec.html doc/footer.html

docs:
	cd src && $(MAKE) docs

# three levels of thoroughness are available for checking the
# correctness of FastBit functions: check, more-check, and full-check.
# The normal check may take about a few seconds on a typical Linux machine,
# more-check takes about ten to fifteen minutes, while full-check takes
# several hours.
more-check:
	cd tests && $(MAKE) more-check

full-check:
	cd tests && $(MAKE) full-check

clean: clean-recursive
	-rm -f win/*.suo win/*.ncb win/*/*.suo win/*/*.ncb win/*.obj win/*.lib win/*.exe win/*.exe.* win/*/*.obj win/*/*.lib win/*/*.exe win/*/*.exe.* win/fastbit.def
	-rm -fr autom4te.cache doc/html doc/latex win/Debug win/Release win/*/Debug win/*/Release win/dll win/*.vcproj.* win/*/*.vcproj.*

distclean: distclean-recursive
	-rm -f $(am__CONFIG_DISTCLEAN_FILES) Makefile win/*.suo win/*.ncb win/*.obj win/*.exe win/*.exe.* win/*/*.suo win/*/*.ncb win/*/*.obj win/*/*.exe win/*/*.exe.*
	-rm -fr autom4te.cache doc/html doc/latex win/Debug win/Release win/*/Debug win/*/Release win/dll win/*.vcproj.* win/*/*.vcproj.*

maintainer-clean: maintainer-clean-am
	-rm -rf ./$(DEPDIR) autom4te.cache
	-rm -f Makefile

#dist-hook:
#	-rm -rf `find $(distdir) \( -name CVS -o \name .svn \)`

.PHONY: docs more-check full-check
