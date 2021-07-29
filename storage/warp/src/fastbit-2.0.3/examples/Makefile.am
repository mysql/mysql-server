AUTOMAKE_OPTIONS=gnu
bin_PROGRAMS = ibis tcapi thula ardea rara tiapi
#EXTRA_PROGRAMS =
ibis_SOURCES = ibis.cpp
ibis_DEPENDENCIES = ../src/libfastbit.la
ibis_LDADD = ../src/libfastbit.la
rara_SOURCES = rara.cpp
rara_DEPENDENCIES = ../src/libfastbit.la
rara_LDADD = ../src/libfastbit.la
thula_SOURCES = thula.cpp
thula_DEPENDENCIES = ../src/libfastbit.la
thula_LDADD = ../src/libfastbit.la
ardea_SOURCES = ardea.cpp
ardea_DEPENDENCIES = ../src/libfastbit.la
ardea_LDADD = ../src/libfastbit.la
tcapi_SOURCES = tcapi.c
tcapi_DEPENDENCIES = ../src/libfastbit.la
tcapi_LDADD = ../src/libfastbit.la
tiapi_SOURCES = tiapi.c
tiapi_DEPENDENCIES = ../src/libfastbit.la
tiapi_LDADD = ../src/libfastbit.la
#
../src/libfastbit.la::
	cd ../src && $(MAKE) libfastbit.la
# has to be linked with $(CXXLINK)
tcapi$(EXEEXT): tcapi.$(OBJEXT) $(tcapi_LDADD)
	$(CXXLINK) tcapi.$(OBJEXT) $(tcapi_LDADD) $(LIBS)
tiapi$(EXEEXT): tiapi.$(OBJEXT) $(tiapi_LDADD)
	$(CXXLINK) tiapi.$(OBJEXT) $(tiapi_LDADD) $(LIBS)

clean-local:
	-rm -rf *.dSYM .ibis

distclean-local:
	-rm -rf *.dSYM .ibis
