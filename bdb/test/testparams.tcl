# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2002
#	Sleepycat Software.  All rights reserved.
#
# $Id: testparams.tcl,v 11.117 2002/09/05 02:30:00 margo Exp $

set subs {bigfile dead env lock log memp mutex recd rep rpc rsrc \
    sdb sdbtest sec si test txn}

set num_test(bigfile)	  2
set num_test(dead)	  7
set num_test(env)	 11
set num_test(lock)	  5
set num_test(log)	  5
set num_test(memp)	  3
set num_test(mutex)	  3
set num_test(recd)	 20
set num_test(rep)	  5
set num_test(rpc)	  5
set num_test(rsrc)	  4
set num_test(sdb)	 12
set num_test(sdbtest)	  2
set num_test(sec)	  2
set num_test(si)	  6
set num_test(test)	101 	
set num_test(txn)	  9

set parms(recd001) 0
set parms(recd002) 0
set parms(recd003) 0
set parms(recd004) 0
set parms(recd005) ""
set parms(recd006) 0
set parms(recd007) ""
set parms(recd008) {4 4}
set parms(recd009) 0
set parms(recd010) 0
set parms(recd011) {200 15 1}
set parms(recd012) {0 49 25 100 5}
set parms(recd013) 100
set parms(recd014) ""
set parms(recd015) ""
set parms(recd016) ""
set parms(recd017) 0
set parms(recd018) 10
set parms(recd019) 50
set parms(recd020) ""
set parms(subdb001) ""
set parms(subdb002) 10000
set parms(subdb003) 1000
set parms(subdb004) ""
set parms(subdb005) 100
set parms(subdb006) 100
set parms(subdb007) ""
set parms(subdb008) ""
set parms(subdb009) ""
set parms(subdb010) ""
set parms(subdb011) {13 10}
set parms(subdb012) ""
set parms(test001) {10000 0 "01" 0}
set parms(test002) 10000
set parms(test003) ""
set parms(test004) {10000 4 0}
set parms(test005) 10000
set parms(test006) {10000 0 6}
set parms(test007) {10000 7}
set parms(test008) {8 0}
set parms(test009) ""
set parms(test010) {10000 5 10}
set parms(test011) {10000 5 11}
set parms(test012)  ""
set parms(test013) 10000
set parms(test014) 10000
set parms(test015) {7500 0}
set parms(test016) 10000
set parms(test017) {0 19 17}
set parms(test018) 10000
set parms(test019) 10000
set parms(test020) 10000
set parms(test021) 10000
set parms(test022) ""
set parms(test023) ""
set parms(test024) 10000
set parms(test025) {10000 0 25}
set parms(test026) {2000 5 26}
set parms(test027) {100}
set parms(test028) ""
set parms(test029) 10000
set parms(test030) 10000
set parms(test031) {10000 5 31}
set parms(test032) {10000 5 32}
set parms(test033) {10000 5 33}
set parms(test034) 10000
set parms(test035) 10000
set parms(test036) 10000
set parms(test037) 100
set parms(test038) {10000 5 38}
set parms(test039) {10000 5 39}
set parms(test040) 10000
set parms(test041) 10000
set parms(test042) 1000
set parms(test043) 10000
set parms(test044) {5 10 0}
set parms(test045) 1000
set parms(test046) ""
set parms(test047) ""
set parms(test048) ""
set parms(test049) ""
set parms(test050) ""
set parms(test051) ""
set parms(test052) ""
set parms(test053) ""
set parms(test054) ""
set parms(test055) ""
set parms(test056) ""
set parms(test057) ""
set parms(test058) ""
set parms(test059) ""
set parms(test060) ""
set parms(test061) ""
set parms(test062) {200 200 62}
set parms(test063) ""
set parms(test064) ""
set parms(test065) ""
set parms(test066) ""
set parms(test067) {1000 67}
set parms(test068) ""
set parms(test069) {50 69}
set parms(test070) {4 2 1000 CONSUME 0 -txn 70}
set parms(test071) {1 1 10000 CONSUME 0 -txn 71}
set parms(test072) {512 20 72}
set parms(test073) {512 50 73}
set parms(test074) {-nextnodup 100 74}
set parms(test075) {75}
set parms(test076) {1000 76}
set parms(test077) {1000 512 77}
set parms(test078) {100 512 78}
set parms(test079) {10000 512 79}
set parms(test080) {80}
set parms(test081) {13 81}
set parms(test082) {-prevnodup 100 82}
set parms(test083) {512 5000 2}
set parms(test084) {10000 84 65536}
set parms(test085) {512 3 10 85}
set parms(test086) ""
set parms(test087) {512 50 87}
set parms(test088) ""
set parms(test089) 1000
set parms(test090) {10000 -txn 90}
set parms(test091) {4 2 1000 0 91}
set parms(test092) {1000}
set parms(test093) {10000 93}
set parms(test094) {10000 10 94}
set parms(test095) {1000 25 95}
set parms(test096) {512 1000 19}
set parms(test097) {500 400}
set parms(test098) ""
set parms(test099) 10000
set parms(test100) {10000 -txn 100}
set parms(test101) {10000 -txn 101}

# RPC server executables.  Each of these is tested (if it exists)
# when running the RPC tests.
set svc_list { berkeley_db_svc berkeley_db_cxxsvc \
    berkeley_db_javasvc }
set rpc_svc berkeley_db_svc

# Shell script tests.  Each list entry is a {directory filename} pair,
# invoked with "/bin/sh filename".
set shelltest_list {
	{ scr001	chk.code }
	{ scr002	chk.def }
	{ scr003	chk.define }
	{ scr004	chk.javafiles }
	{ scr005	chk.nl }
	{ scr006	chk.offt }
	{ scr007	chk.proto }
	{ scr008	chk.pubdef }
	{ scr009	chk.srcfiles }
	{ scr010	chk.str }
	{ scr011	chk.tags }
	{ scr012	chk.vx_code }
	{ scr013	chk.stats }
	{ scr014	chk.err }
	{ scr015	chk.cxxtests }
	{ scr016	chk.javatests }
	{ scr017	chk.db185 }
	{ scr018	chk.comma }
	{ scr019	chk.include }
	{ scr020	chk.inc }
	{ scr021	chk.flags }
	{ scr022	chk.rr }
}
