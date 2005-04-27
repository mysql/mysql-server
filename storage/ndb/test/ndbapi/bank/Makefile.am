
ndbtest_PROGRAMS = testBank bankSumAccounts bankValidateAllGLs bankMakeGL bankTransactionMaker bankCreator bankTimer

noinst_LIBRARIES = libbank.a

libbank_a_SOURCES = Bank.cpp BankLoad.cpp Bank.hpp

testBank_SOURCES = testBank.cpp
bankSumAccounts_SOURCES = bankSumAccounts.cpp
bankValidateAllGLs_SOURCES = bankValidateAllGLs.cpp 
bankMakeGL_SOURCES = bankMakeGL.cpp 
bankTransactionMaker_SOURCES = bankTransactionMaker.cpp 
bankCreator_SOURCES = bankCreator.cpp  
bankTimer_SOURCES = bankTimer.cpp

LDADD_LOC = $(noinst_LIBRARIES)

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_ndbapitest.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%

windoze-dsp:
