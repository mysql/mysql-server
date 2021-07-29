# $Id$
# Makefile for mingw32-make on windows using MinGW g++ port
#
# If you are attempting to do cross compiling, please use xMinGW.mak!
#
CXX=g++.exe
OPT=-g -O0
#OPT=-O5
INC=-I "C:/MinGW/include" -I "C:/MinGW/msys/1.0/include" -I . -I ../src -I "pthreads-w32-2-8-0-release"
DEF=-DFASTBIT_MAX_WAIT_TIME=3 -DWITHOUT_FASTBIT_CONFIG_H
LIB=-Lpthreads-w32-2-8-0-release -lpthreadGC2 -lm
TESTDIR=tmp

CCFLAGS=$(DEF) $(INC) $(OPT)
# as of Dec 2011, make from MSYS needs the unix sh
# SHELL=cmd.exe ## comment out to use the default unix sh
RM=rm -r -f
#RM = del /s /f
#
OBJ =  parth3d.o parth3da.o parth3db.o parth3dw.o \
 array_t.o \
 bitvector.o \
 bitvector64.o \
 blob.o \
 bundle.o \
 capi.o \
 category.o \
 colValues.o \
 column.o \
 countQuery.o \
 dictionary.o \
 fileManager.o \
 ibin.o \
 jnatural.o \
 jrange.o \
 quaere.o \
 bord.o \
 bordm.o \
 tafel.o \
 mensa.o \
 party.o \
 part.o \
 parth.o \
 parth2d.o \
 parti.o \
 icegale.o \
 icentre.o \
 icmoins.o \
 idbak.o \
 idbak2.o \
 idirekte.o \
 ifade.o \
 ikeywords.o \
 imesa.o \
 index.o \
 irange.o \
 irelic.o \
 iroster.o \
 isapid.o \
 isbiad.o \
 iskive.o islice.o \
 ixambit.o \
 ixbylt.o \
 ixfuge.o \
 ixfuzz.o \
 ixpack.o \
 ixpale.o \
 ixzona.o \
 ixzone.o \
 filter.o \
 meshQuery.o \
 fromClause.o \
 fromLexer.o \
 fromParser.o \
 selectClause.o \
 selectLexer.o \
 selectParser.o \
 whereClause.o \
 whereLexer.o \
 whereParser.o \
 qExpr.o \
 query.o \
 resource.o \
 rids.o \
 utilidor.o \
 util.o

#
all: ibis ardea rara thula tcapi
IBISEXE=./ibis.exe
ARDEAEXE=./ardea.exe

lib: libfastbit.a
libfastbit.a: $(OBJ)
	ar ruv libfastbit.a $(OBJ)

ibis: ibis.exe
ibis.exe: ibis.o libfastbit.a
	$(CXX) $(OPT) -o ibis ibis.o libfastbit.a $(LIB)

thula: thula.exe
thula.exe: thula.o libfastbit.a
	$(CXX) $(OPT) -o thula thula.o libfastbit.a $(LIB)

rara: rara.exe
rara.exe: rara.o libfastbit.a
	$(CXX) $(OPT) -o rara rara.o libfastbit.a $(LIB)

ardea: ardea.exe
ardea.exe: ardea.o libfastbit.a
	$(CXX) $(OPT) -o ardea ardea.o libfastbit.a $(LIB)

dll: fastbit.dll
fastbit.a: fastbit.dll
fastbit.dll: $(FRC)
	make -f MinGW.mak DEF="$(DEF) -DCXX_USE_DLL -DDLL_EXPORT" $(OBJ)
	$(CXX) -shared -o $@ $(OBJ) $(LIB)
	dlltool -z fastbit.def $(OBJ)
	dlltool -k --dllname fastbit.dll --output-lib fastbit.a --def fastbit.def
# -Wl,-soname,$@
trydll: trydll.cpp fastbit.dll
	$(CXX) $(INC) $(OPT) -DCXX_USE_DLL -o trydll trydll.cpp fastbit.a $(LIB)

tcapi: ../examples/tcapi.c ../src/capi.h fastbit.dll
	$(CXX) $(INC) $(OPT) -o tcapi ../examples/tcapi.c libfastbit.a $(LIB)

check-ibis: $(IBISEXE) $(TESTDIR)/t1/-part.txt $(TESTDIR)/rowlist
	@$(RM) $(TESTDIR)/hist0 $(TESTDIR)/hist1 $(TESTDIR)/hist2
	@$(IBISEXE) -d $(TESTDIR)/t1 -q "where a = 0"
	@echo Expected 2 hits from the above query
	@echo
	@$(IBISEXE) -d $(TESTDIR)/t1 -q "where a = b and c < exp(log(9.5))"
	@echo Expected 20 hits from the above query
	@echo
	@$(IBISEXE) -d $(TESTDIR)/t1 -q "where c < 2"
	@echo Expected 4 hits from the above query
	@echo
	@$(IBISEXE) -d $(TESTDIR)/t1 -p "c : c>80" > $(TESTDIR)/hist0
	@echo Please compare the content in $(TESTDIR)/hist0 with ../tests/hist0
	@echo
	@$(IBISEXE) -d $(TESTDIR)/t1 -v -t 5
	@echo Expected no error from the above tests
	@echo
$(TESTDIR)/t1/-part.txt: $(ARDEAEXE) ../tests/test0.csv
	$(RM) $(TESTDIR)/t1
	$(ARDEAEXE) -d $(TESTDIR)/t1 -m "a:int, b:float, c:ushort" -t ../tests/test0.csv
	$(ARDEAEXE) -d $(TESTDIR)/t1 -m "a:int, b:float, c:ushort" -t ../tests/test0.csv
$(TESTDIR)/rowlist: $(TESTDIR)/t1/-part.txt
	echo 0 > $(TESTDIR)/rowlist; echo 99 >> $(TESTDIR)/rowlist;

clean:
	$(RM) *.o core b?_? *.dll *.lib *.exe *.a *.so *.suo *.ncb *.exp *.pdb
clean-all: clean
	$(RM) Debug Release dll tmp pthreads-w32-2-8-0-release pthreads-w32-2-8-0-release.tar.gz

pthreads-w32: pthreads-w32-2-8-0-release/libpthreadGC2.a
pthreads-w32-2-8-0-release.tar.gz:
	wget ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-8-0-release.tar.gz
pthreads-w32-2-8-0-release: pthreads-w32-2-8-0-release.tar.gz
	tar xzf pthreads-w32-2-8-0-release.tar.gz
pthreads-w32-2-8-0-release/libpthreadGC2.a: pthreads-w32-2-8-0-release
	cd pthreads-w32-2-8-0-release && make clean PTW32_FLAGS=-DPTW32_BUILD GC

force:

#suffixes
.SUFFIXES: .o .cpp .h
# #
# # rules to generate .h and .cpp files from predicate.y and predicate.l
# predicate: predicate.tab.h predicate.tab.cpp predicate.yy.cpp
# predicate.tab.cpp predicate.tab.h: predicate.y
# 	yacc -d -b predicate predicate.y
# 	mv predicate.tab.c predicate.tab.cpp
# #	touch predicate.tab.cpp
# predicate.yy.cpp: predicate.l
# 	lex predicate.l
# 	sed -e 's/^yylex/int yylex/' lex.yy.c > predicate.yy.cpp
# 	rm lex.yy.c
# predicate.h: predicate.tab.h predicate.yy.cpp
# suffixes based rules
.cpp.o:
	$(CXX) $(CCFLAGS) -c $<
############################################################
# dependencies generated with g++ -MM
trydll.exe: ../src/ibis.h ../src/meshQuery.h ../src/query.h \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/bundle.h ../src/colValues.h ../src/rids.h
array_t.o: ../src/array_t.cpp ../src/array_t.h ../src/fileManager.h \
  ../src/util.h ../src/const.h  ../src/horometer.h
	$(CXX) $(CCFLAGS) -c -o array_t.o ../src/array_t.cpp
bitvector.o: ../src/bitvector.cpp ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/const.h \
  ../src/horometer.h
	$(CXX) $(CCFLAGS) -c -o bitvector.o ../src/bitvector.cpp
bitvector64.o: ../src/bitvector64.cpp ../src/bitvector64.h \
  ../src/array_t.h ../src/fileManager.h ../src/util.h ../src/const.h \
  ../src/horometer.h ../src/bitvector.h
	$(CXX) $(CCFLAGS) -c -o bitvector64.o ../src/bitvector64.cpp
blob.o: ../src/blob.cpp ../src/blob.h ../src/table.h ../src/const.h \
  ../src/fastbit-config.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/horometer.h ../src/column.h \
  ../src/qExpr.h ../src/part.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o blob.o ../src/blob.cpp
bord.o: ../src/bord.cpp ../src/tab.h ../src/table.h ../src/const.h \
  ../src/bord.h ../src/util.h ../src/part.h ../src/countQuery.h \
  ../src/column.h ../src/qExpr.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h ../src/bundle.h \
  ../src/colValues.h
	$(CXX) $(CCFLAGS) -c -o bord.o ../src/bord.cpp
bordm.o: ../src/bordm.cpp ../src/tab.h ../src/table.h ../src/const.h \
  ../src/bord.h ../src/util.h ../src/part.h ../src/countQuery.h \
  ../src/column.h ../src/qExpr.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h ../src/bundle.h \
  ../src/colValues.h
	$(CXX) $(CCFLAGS) -c -o bordm.o ../src/bordm.cpp
bundle.o: ../src/bundle.cpp ../src/bundle.h ../src/util.h ../src/const.h \
   ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/query.h ../src/part.h ../src/column.h \
  ../src/table.h ../src/qExpr.h ../src/bitvector.h ../src/resource.h \
  ../src/utilidor.h ../src/whereClause.h ../src/colValues.h
	$(CXX) $(CCFLAGS) -c -o bundle.o ../src/bundle.cpp
capi.o: ../src/capi.cpp ../src/capi.h  \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h ../src/bundle.h \
  ../src/colValues.h ../src/tafel.h
	$(CXX) $(CCFLAGS) -c -o capi.o ../src/capi.cpp
category.o: ../src/category.cpp ../src/part.h ../src/column.h \
  ../src/table.h ../src/const.h  ../src/qExpr.h \
  ../src/util.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/category.h ../src/irelic.h ../src/index.h ../src/ikeywords.h
	$(CXX) $(CCFLAGS) -c -o category.o ../src/category.cpp
colValues.o: ../src/colValues.cpp ../src/bundle.h ../src/util.h \
  ../src/const.h  ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/query.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/qExpr.h ../src/bitvector.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h \
  ../src/colValues.h
	$(CXX) $(CCFLAGS) -c -o colValues.o ../src/colValues.cpp
column.o: ../src/column.cpp ../src/resource.h ../src/util.h \
  ../src/const.h  ../src/category.h \
  ../src/irelic.h ../src/index.h ../src/qExpr.h ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/column.h ../src/table.h ../src/part.h ../src/utilidor.h \
  ../src/iroster.h
	$(CXX) $(CCFLAGS) -c -o column.o ../src/column.cpp
countQuery.o: ../src/countQuery.cpp ../src/countQuery.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/const.h  \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/whereClause.h ../src/selectClause.h \
  ../src/query.h
	$(CXX) $(CCFLAGS) -c ../src/countQuery.cpp
dictionary.o: ../src/dictionary.cpp ../src/dictionary.h ../src/util.h \
  ../src/const.h ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c ../src/dictionary.cpp
fileManager.o: ../src/fileManager.cpp ../src/fileManager.h ../src/util.h \
  ../src/const.h  ../src/resource.h \
  ../src/array_t.h ../src/horometer.h
	$(CXX) $(CCFLAGS) -c -o fileManager.o ../src/fileManager.cpp
filter.o: ../src/filter.cpp ../src/filter.h ../src/query.h \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o filter.o ../src/filter.cpp
ibin.o: ../src/ibin.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h \
  ../src/bitvector64.h
	$(CXX) $(CCFLAGS) -c -o ibin.o ../src/ibin.cpp
icegale.o: ../src/icegale.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o icegale.o ../src/icegale.cpp
icentre.o: ../src/icentre.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o icentre.o ../src/icentre.cpp
icmoins.o: ../src/icmoins.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o icmoins.o ../src/icmoins.cpp
idbak.o: ../src/idbak.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o idbak.o ../src/idbak.cpp
idbak2.o: ../src/idbak2.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o idbak2.o ../src/idbak2.cpp
idirekte.o: ../src/idirekte.cpp ../src/idirekte.h ../src/index.h \
  ../src/qExpr.h ../src/util.h ../src/const.h  \
  ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/part.h ../src/column.h ../src/table.h \
  ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o idirekte.o ../src/idirekte.cpp
ifade.o: ../src/ifade.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ifade.o ../src/ifade.cpp
ikeywords.o: ../src/ikeywords.cpp ../src/ikeywords.h ../src/index.h \
  ../src/qExpr.h ../src/util.h ../src/const.h  \
  ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/category.h ../src/irelic.h ../src/column.h \
  ../src/table.h ../src/iroster.h ../src/part.h ../src/resource.h \
  ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ikeywords.o ../src/ikeywords.cpp
imesa.o: ../src/imesa.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o imesa.o ../src/imesa.cpp
index.o: ../src/index.cpp ../src/index.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/ibin.h \
  ../src/idirekte.h ../src/ikeywords.h ../src/category.h ../src/irelic.h \
  ../src/column.h ../src/table.h ../src/part.h ../src/resource.h \
  ../src/utilidor.h ../src/bitvector64.h
	$(CXX) $(CCFLAGS) -c -o index.o ../src/index.cpp
irange.o: ../src/irange.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o irange.o ../src/irange.cpp
irelic.o: ../src/irelic.cpp ../src/bitvector64.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/const.h \
  ../src/horometer.h ../src/irelic.h \
  ../src/index.h ../src/qExpr.h ../src/bitvector.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o irelic.o ../src/irelic.cpp
iroster.o: ../src/iroster.cpp ../src/iroster.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/const.h \
  ../src/horometer.h ../src/column.h \
  ../src/table.h ../src/qExpr.h ../src/bitvector.h ../src/part.h \
  ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o iroster.o ../src/iroster.cpp
isapid.o: ../src/isapid.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o isapid.o ../src/isapid.cpp
isbiad.o: ../src/isbiad.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o isbiad.o ../src/isbiad.cpp
iskive.o: ../src/iskive.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o iskive.o ../src/iskive.cpp
islice.o: ../src/islice.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o islice.o ../src/islice.cpp
ixambit.o: ../src/ixambit.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixambit.o ../src/ixambit.cpp
ixbylt.o: ../src/ixbylt.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixbylt.o ../src/ixbylt.cpp
ixfuge.o: ../src/ixfuge.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixfuge.o ../src/ixfuge.cpp
ixfuzz.o: ../src/ixfuzz.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixfuzz.o ../src/ixfuzz.cpp
ixpack.o: ../src/ixpack.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixpack.o ../src/ixpack.cpp
ixpale.o: ../src/ixpale.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixpale.o ../src/ixpale.cpp
ixzona.o: ../src/ixzona.cpp ../src/irelic.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixzona.o ../src/ixzona.cpp
ixzone.o: ../src/ixzone.cpp ../src/ibin.h ../src/index.h ../src/qExpr.h \
  ../src/util.h ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o ixzone.o ../src/ixzone.cpp
jnatural.o: ../src/jnatural.cpp ../src/jnatural.h ../src/quaere.h ../src/table.h \
  ../src/const.h  ../src/part.h ../src/column.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o jnatural.o ../src/jnatural.cpp
jrange.o: ../src/jrange.cpp ../src/jrange.h ../src/quaere.h ../src/table.h \
  ../src/const.h  ../src/part.h ../src/column.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o jrange.o ../src/jrange.cpp
quaere.o: ../src/quaere.cpp ../src/jnatural.h ../src/jrange.h ../src/quaere.h \
  ../src/const.h  ../src/part.h ../src/column.h ../src/table.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/query.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o quaere.o ../src/quaere.cpp
mensa.o: ../src/mensa.cpp ../src/tab.h ../src/table.h ../src/const.h \
  ../src/bord.h ../src/util.h ../src/part.h \
  ../src/column.h ../src/qExpr.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/mensa.h ../src/countQuery.h \
  ../src/whereClause.h ../src/index.h ../src/category.h ../src/irelic.h \
  ../src/selectClause.h
	$(CXX) $(CCFLAGS) -c -o mensa.o ../src/mensa.cpp
meshQuery.o: ../src/meshQuery.cpp ../src/meshQuery.h ../src/query.h \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
   ../src/qExpr.h ../src/util.h ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o meshQuery.o ../src/meshQuery.cpp
part.o: ../src/part.cpp ../src/qExpr.h ../src/util.h ../src/const.h \
   ../src/category.h ../src/irelic.h \
  ../src/index.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/column.h ../src/table.h ../src/query.h \
  ../src/part.h ../src/resource.h ../src/utilidor.h ../src/whereClause.h \
  ../src/countQuery.h ../src/iroster.h ../src/twister.h
	$(CXX) $(CCFLAGS) -c -o part.o ../src/part.cpp
parth.o: ../src/parth.cpp ../src/index.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/countQuery.h ../src/part.h ../src/column.h ../src/table.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth.o ../src/parth.cpp
parth2d.o: ../src/parth2d.cpp ../src/index.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/countQuery.h ../src/part.h ../src/column.h ../src/table.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth2d.o ../src/parth2d.cpp
parth3d.o: ../src/parth3d.cpp ../src/index.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/countQuery.h ../src/part.h ../src/column.h ../src/table.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth3d.o ../src/parth3d.cpp
parth3da.o: ../src/parth3da.cpp ../src/countQuery.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/const.h  \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth3da.o ../src/parth3da.cpp
parth3db.o: ../src/parth3db.cpp ../src/countQuery.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/const.h  \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth3db.o ../src/parth3db.cpp
parth3dw.o: ../src/parth3dw.cpp ../src/countQuery.h ../src/part.h \
  ../src/column.h ../src/table.h ../src/const.h  \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/horometer.h ../src/resource.h \
  ../src/utilidor.h ../src/whereClause.h
	$(CXX) $(CCFLAGS) -c -o parth3dw.o ../src/parth3dw.cpp
parti.o: ../src/parti.cpp ../src/part.h ../src/column.h ../src/table.h \
  ../src/const.h  ../src/qExpr.h ../src/util.h \
  ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/category.h ../src/irelic.h ../src/index.h ../src/selectClause.h
	$(CXX) $(CCFLAGS) -c -o parti.o ../src/parti.cpp
party.o: ../src/party.cpp ../src/part.h ../src/column.h ../src/table.h \
  ../src/const.h  ../src/qExpr.h ../src/util.h \
  ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h ../src/iroster.h \
  ../src/bitvector64.h
	$(CXX) $(CCFLAGS) -c -o party.o ../src/party.cpp
qExpr.o: ../src/qExpr.cpp ../src/util.h ../src/const.h \
   ../src/part.h ../src/column.h ../src/table.h \
  ../src/qExpr.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o qExpr.o ../src/qExpr.cpp
query.o: ../src/query.cpp ../src/query.h ../src/part.h ../src/column.h \
  ../src/table.h ../src/const.h  ../src/qExpr.h \
  ../src/util.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/whereClause.h ../src/bundle.h ../src/colValues.h ../src/ibin.h \
  ../src/index.h ../src/iroster.h ../src/irelic.h ../src/bitvector64.h
	$(CXX) $(CCFLAGS) -c -o query.o ../src/query.cpp
resource.o: ../src/resource.cpp ../src/util.h ../src/const.h \
   ../src/resource.h
	$(CXX) $(CCFLAGS) -c -o resource.o ../src/resource.cpp
rids.o: ../src/rids.cpp ../src/rids.h ../src/utilidor.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/const.h \
   ../src/horometer.h
	$(CXX) $(CCFLAGS) -c -o rids.o ../src/rids.cpp
fromClause.o: ../src/fromClause.cpp ../src/part.h ../src/column.h \
  ../src/table.h ../src/const.h  ../src/qExpr.h \
  ../src/util.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/fromLexer.h ../src/fromParser.hh ../src/stack.hh \
  ../src/fromClause.h ../src/location.hh ../src/position.hh \
  ./FlexLexer.h
	$(CXX) $(CCFLAGS) -c -o fromClause.o ../src/fromClause.cpp
fromLexer.o: ../src/fromLexer.cc ./FlexLexer.h ../src/fromLexer.h \
  ../src/fromParser.hh ../src/stack.hh ../src/fromClause.h \
  ../src/qExpr.h ../src/util.h ../src/const.h  \
  ../src/location.hh ../src/position.hh
	$(CXX) $(CCFLAGS) -c -o fromLexer.o ../src/fromLexer.cc
fromParser.o: ../src/fromParser.cc ../src/fromParser.hh \
  ../src/stack.hh ../src/fromClause.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/location.hh \
  ../src/position.hh ../src/fromLexer.h ./FlexLexer.h
	$(CXX) $(CCFLAGS) -c -o fromParser.o ../src/fromParser.cc
selectClause.o: ../src/selectClause.cpp ../src/part.h ../src/column.h \
  ../src/table.h ../src/const.h  ../src/qExpr.h \
  ../src/util.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/selectLexer.h ../src/selectParser.hh ../src/stack.hh \
  ../src/selectClause.h ../src/location.hh ../src/position.hh \
  ./FlexLexer.h
	$(CXX) $(CCFLAGS) -c -o selectClause.o ../src/selectClause.cpp
selectLexer.o: ../src/selectLexer.cc ./FlexLexer.h ../src/selectLexer.h \
  ../src/selectParser.hh ../src/stack.hh ../src/selectClause.h \
  ../src/qExpr.h ../src/util.h ../src/const.h  \
  ../src/location.hh ../src/position.hh
	$(CXX) $(CCFLAGS) -c -o selectLexer.o ../src/selectLexer.cc
selectParser.o: ../src/selectParser.cc ../src/selectParser.hh \
  ../src/stack.hh ../src/selectClause.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/location.hh \
  ../src/position.hh ../src/selectLexer.h ./FlexLexer.h
	$(CXX) $(CCFLAGS) -c -o selectParser.o ../src/selectParser.cc
tafel.o: ../src/tafel.cpp ../src/tafel.h ../src/table.h ../src/const.h \
   ../src/bitvector.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/horometer.h ../src/part.h \
  ../src/column.h ../src/qExpr.h ../src/resource.h ../src/utilidor.h
	$(CXX) $(CCFLAGS) -c -o tafel.o ../src/tafel.cpp
util.o: ../src/util.cpp ../src/util.h ../src/const.h \
   ../src/horometer.h ../src/resource.h
	$(CXX) $(CCFLAGS) -c -o util.o ../src/util.cpp
utilidor.o: ../src/utilidor.cpp ../src/utilidor.h ../src/array_t.h \
  ../src/fileManager.h ../src/util.h ../src/const.h \
   ../src/horometer.h
	$(CXX) $(CCFLAGS) -c -o utilidor.o ../src/utilidor.cpp
whereClause.o: ../src/whereClause.cpp ../src/part.h ../src/column.h \
  ../src/table.h ../src/const.h  ../src/qExpr.h \
  ../src/util.h ../src/bitvector.h ../src/array_t.h ../src/fileManager.h \
  ../src/horometer.h ../src/resource.h ../src/utilidor.h \
  ../src/whereLexer.h ../src/whereParser.hh ../src/stack.hh \
  ../src/whereClause.h ../src/location.hh ../src/position.hh \
  ./FlexLexer.h ../src/selectClause.h
	$(CXX) $(CCFLAGS) -c -o whereClause.o ../src/whereClause.cpp
whereLexer.o: ../src/whereLexer.cc ./FlexLexer.h ../src/whereLexer.h \
  ../src/whereParser.hh ../src/stack.hh ../src/whereClause.h \
  ../src/qExpr.h ../src/util.h ../src/const.h  \
  ../src/location.hh ../src/position.hh
	$(CXX) $(CCFLAGS) -c -o whereLexer.o ../src/whereLexer.cc
whereParser.o: ../src/whereParser.cc ../src/whereParser.hh \
  ../src/stack.hh ../src/whereClause.h ../src/qExpr.h ../src/util.h \
  ../src/const.h  ../src/location.hh \
  ../src/position.hh ../src/whereLexer.h ./FlexLexer.h
	$(CXX) $(CCFLAGS) -c -o whereParser.o ../src/whereParser.cc
ardea.o: ../examples/ardea.cpp ../src/table.h ../src/const.h \
   ../src/resource.h ../src/util.h
	$(CXX) $(CCFLAGS) -c -o ardea.o ../examples/ardea.cpp
ibis.o: ../examples/ibis.cpp ../src/ibis.h ../src/countQuery.h \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h \
  ../src/meshQuery.h ../src/query.h ../src/bundle.h ../src/colValues.h \
  ../src/quaere.h ../src/rids.h ../src/mensa.h
	$(CXX) $(CCFLAGS) -c -o ibis.o ../examples/ibis.cpp
rara.o: ../examples/rara.cpp ../src/ibis.h ../src/countQuery.h \
  ../src/part.h ../src/column.h ../src/table.h ../src/const.h \
  ../src/qExpr.h ../src/util.h ../src/bitvector.h \
  ../src/array_t.h ../src/fileManager.h ../src/horometer.h \
  ../src/resource.h ../src/utilidor.h ../src/whereClause.h \
  ../src/meshQuery.h ../src/query.h ../src/bundle.h ../src/colValues.h \
  ../src/quaere.h ../src/rids.h
	$(CXX) $(CCFLAGS) -c  -o rara.o ../examples/rara.cpp
thula.o: ../examples/thula.cpp ../src/table.h ../src/const.h \
  ../src/resource.h ../src/util.h ../src/mensa.h \
  ../src/table.h ../src/fileManager.h
	$(CXX) $(CCFLAGS) -c -o thula.o ../examples/thula.cpp
tcapi.o: ../examples/tcapi.c ../src/capi.h
	$(CXX) $(CCFLAGS) -c -o tcapi.o ../examples/tcapi.c
