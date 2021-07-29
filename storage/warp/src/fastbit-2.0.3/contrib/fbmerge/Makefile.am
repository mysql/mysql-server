AUTOMAKE_OPTIONS=gnu
EXTRA_DIST = fbmerge.sln fbmerge.vcproj

bin_PROGRAMS = fbmerge
fbmerge_SOURCES = fbmerge.cpp
fbmerge_DEPENDENCIES = ../../src/libfastbit.la
fbmerge_LDADD = ../../src/libfastbit.la
#
../../src/libfastbit.la::
	cd ../../src && $(MAKE) libfastbit.la

distclean-local:
	-rm -rf .ibis
