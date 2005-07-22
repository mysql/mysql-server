# See the file LICENSE for redistribution information.
#
# Copyright (c) 2000-2004
#	Sleepycat Software.  All rights reserved.
#
# $Id: testparams.tcl,v 11.200 2004/10/12 16:22:14 sue Exp $

global one_test
global serial_tests
set serial_tests {rep002 rep005}

set subs {bigfile dead env fop lock log memp mutex recd rep rpc rsrc\
	sdb sdbtest sec si test txn}

set test_names(bigfile)	[list bigfile001 bigfile002]
set test_names(dead)    [list dead001 dead002 dead003 dead004 dead005 dead006 \
    dead007]
set test_names(elect)	[list rep002 rep005 rep016 rep020 rep022]
set test_names(env)	[list env001 env002 env003 env004 env005 env006 \
    env007 env008 env009 env010 env011]
set test_names(fop)	[list fop001 fop002 fop003 fop004 fop005 fop006]
set test_names(lock)    [list lock001 lock002 lock003 lock004 lock005 lock006]
set test_names(log)     [list log001 log002 log003 log004 log005 log006]
set test_names(memp)	[list memp001 memp002 memp003 memp004]
set test_names(mutex)	[list mutex001 mutex002 mutex003]
set test_names(recd)	[list recd001 recd002 recd003 recd004 recd005 recd006 \
    recd007 recd008 recd009 recd010 recd011 recd012 recd013 recd014 recd015 \
    recd016 recd017 recd018 recd019 recd020 ]
set test_names(rep)	[list rep001 rep002 rep003 rep005 rep006 rep007 \
    rep008 rep009 rep010 rep011 rep012 rep013 rep014 rep015 rep016 rep017 \
    rep018 rep019 rep020 rep021 rep022 rep023 rep024 rep026 rep027 rep028 \
    rep029 rep030 rep031 rep032 rep033 rep034 rep035 rep036 rep037]
set test_names(rpc)	[list rpc001 rpc002 rpc003 rpc004 rpc005 rpc006]
set test_names(rsrc)	[list rsrc001 rsrc002 rsrc003 rsrc004]
set test_names(sdb)	[list sdb001 sdb002 sdb003 sdb004 sdb005 sdb006 \
    sdb007 sdb008 sdb009 sdb010 sdb011 sdb012]
set test_names(sdbtest)	[list sdbtest001 sdbtest002]
set test_names(sec)	[list sec001 sec002]
set test_names(si)	[list si001 si002 si003 si004 si005]
set test_names(test)	[list test001 test002 test003 test004 test005 \
    test006 test007 test008 test009 test010 test011 test012 test013 test014 \
    test015 test016 test017 test018 test019 test020 test021 test022 test023 \
    test024 test025 test026 test027 test028 test029 test030 test031 test032 \
    test033 test034 test035 test036 test037 test038 test039 test040 test041 \
    test042 test043 test044 test045 test046 test047 test048 test049 test050 \
    test051 test052 test053 test054 test055 test056 test057 test058 test059 \
    test060 test061 test062 test063 test064 test065 test066 test067 test068 \
    test069 test070 test071 test072 test073 test074 test076 test077 \
    test078 test079 test081 test082 test083 test084 test085 test086 \
    test087 test088 test089 test090 test091 test092 test093 test094 test095 \
    test096 test097 test098 test099 test100 test101 test102 test103 test107 \
    test109 ]
set test_names(txn)	[list txn001 txn002 txn003 txn004 txn005 txn006 \
    txn007 txn008 txn009 txn010 txn011]

set rpc_tests(berkeley_db_svc) [concat $test_names(test) $test_names(sdb)]
set rpc_tests(berkeley_db_cxxsvc) $test_names(test)
set rpc_tests(berkeley_db_javasvc) $test_names(test)

# JE tests are a subset of regular RPC tests -- exclude these ones.
# be fixable by modifying tests dealing with unsorted duplicates, second line
# will probably never work unless certain features are added to JE (record
# numbers, bulk get, etc.).
set je_exclude {(?x)    # Turn on extended syntax
	test(010|026|027|028|030|031|032|033|034|   # These should be fixable by
	     035|039|041|046|047|054|056|057|062|   # modifying tests to avoid
	     066|073|081|085)|                      # unsorted dups, etc.

	test(011|017|018|022|023|024|029|040|049|   # Not expected to work with
	     062|083|095)                           # JE until / unless features
						    # are added to JE (record
						    # numbers, bulk gets, etc.)
}
set rpc_tests(berkeley_dbje_svc) [lsearch -all -inline -not -regexp \
                                  $rpc_tests(berkeley_db_svc) $je_exclude]

# Source all the tests, whether we're running one or many.
foreach sub $subs {
	foreach test $test_names($sub) {
		source $test_path/$test.tcl
	}
}

# Reset test_names if we're running only one test.
if { $one_test != "ALL" } {
	foreach sub $subs {
		set test_names($sub) ""
	}
	set type [string trim $one_test 0123456789]
	set test_names($type) [list $one_test]
}

source $test_path/archive.tcl
source $test_path/byteorder.tcl
source $test_path/dbm.tcl
source $test_path/foputils.tcl
source $test_path/hsearch.tcl
source $test_path/join.tcl
source $test_path/logtrack.tcl
source $test_path/ndbm.tcl
source $test_path/parallel.tcl
source $test_path/reputils.tcl
source $test_path/sdbutils.tcl
source $test_path/shelltest.tcl
source $test_path/sijointest.tcl
source $test_path/siutils.tcl
source $test_path/testutils.tcl
source $test_path/upgrade.tcl

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
set parms(rep001) {1000 "001"}
set parms(rep002) {10 3 "002"}
set parms(rep003) "003"
set parms(rep005) ""
set parms(rep006) {1000 "006"}
set parms(rep007) {10 "007"}
set parms(rep008) {10 "008"}
set parms(rep009) {10 "009"}
set parms(rep010) {100 "010"}
set parms(rep011) "011"
set parms(rep012) {10 "012"}
set parms(rep013) {10 "013"}
set parms(rep014) {10 "014"}
set parms(rep015) {100 "015" 3}
set parms(rep016) ""
set parms(rep017) {10 "017"}
set parms(rep018) {10 "018"}
set parms(rep019) {3 "019"}
set parms(rep020) ""
set parms(rep021) {3 "021"}
set parms(rep022) ""
set parms(rep023) {10 "023"}
set parms(rep024) {1000 "024"}
set parms(rep026) ""
set parms(rep027) {1000 "027"}
set parms(rep028) {100 "028"}
set parms(rep029) {200 "029"}
set parms(rep030) {500 "030"}
set parms(rep031) {200 "031"}
set parms(rep032) {200 "032"}
set parms(rep033) {200 "033"}
set parms(rep034) {2 "034"}
set parms(rep035) {100 "035"}
set parms(rep036) {200 "036"}
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
set parms(sdb001) ""
set parms(sdb002) 10000
set parms(sdb003) 1000
set parms(sdb004) ""
set parms(sdb005) 100
set parms(sdb006) 100
set parms(sdb007) ""
set parms(sdb008) ""
set parms(sdb009) ""
set parms(sdb010) ""
set parms(sdb011) {13 10}
set parms(sdb012) ""
set parms(si001) {200 1}
set parms(si002) {200 2}
set parms(si003) {200 3}
set parms(si004) {200 4}
set parms(si005) {200 5}
set parms(test001) {10000 0 0 "001"}
set parms(test002) 10000
set parms(test003) ""
set parms(test004) {10000 "004" 0}
set parms(test005) 10000
set parms(test006) {10000 0 "006" 5}
set parms(test007) {10000 "007" 5}
set parms(test008) {"008" 0}
set parms(test009) ""
set parms(test010) {10000 5 "010"}
set parms(test011) {10000 5 "011"}
set parms(test012)  ""
set parms(test013) 10000
set parms(test014) 10000
set parms(test015) {7500 0}
set parms(test016) 10000
set parms(test017) {0 19 "017"}
set parms(test018) 10000
set parms(test019) 10000
set parms(test020) 10000
set parms(test021) 10000
set parms(test022) ""
set parms(test023) ""
set parms(test024) 10000
set parms(test025) {10000 0 "025"}
set parms(test026) {2000 5 "026"}
set parms(test027) {100}
set parms(test028) ""
set parms(test029) 10000
set parms(test030) 10000
set parms(test031) {10000 5 "031"}
set parms(test032) {10000 5 "032"}
set parms(test033) {10000 5 "033"}
set parms(test034) 10000
set parms(test035) 10000
set parms(test036) 10000
set parms(test037) 100
set parms(test038) {10000 5 "038"}
set parms(test039) {10000 5 "039"}
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
set parms(test062) {200 200 "062"}
set parms(test063) ""
set parms(test064) ""
set parms(test065) ""
set parms(test066) ""
set parms(test067) {1000 "067"}
set parms(test068) ""
set parms(test069) {50 "069"}
set parms(test070) {4 2 1000 CONSUME 0 -txn "070"}
set parms(test071) {1 1 10000 CONSUME 0 -txn "071"}
set parms(test072) {512 20 "072"}
set parms(test073) {512 50 "073"}
set parms(test074) {-nextnodup 100 "074"}
set parms(test076) {1000 "076"}
set parms(test077) {1000 "077"}
set parms(test078) {100 512 "078"}
set parms(test079) {10000 512 "079" 20}
set parms(test081) {13 "081"}
set parms(test082) {-prevnodup 100 "082"}
set parms(test083) {512 5000 2}
set parms(test084) {10000 "084" 65536}
set parms(test085) {512 3 10 "085"}
set parms(test086) ""
set parms(test087) {512 50 "087"}
set parms(test088) ""
set parms(test089) 1000
set parms(test090) {10000 "090"}
set parms(test091) {4 2 1000 0 "091"}
set parms(test092) {1000}
set parms(test093) {10000 "093"}
set parms(test094) {10000 10 "094"}
set parms(test095) {"095"}
set parms(test096) {512 1000 19}
set parms(test097) {500 400}
set parms(test098) ""
set parms(test099) 10000
set parms(test100) {10000 "100"}
set parms(test101) {1000 -txn "101"}
set parms(test102) {1000 "102"}
set parms(test103) {100 4294967250 "103"}
set parms(test107) ""
set parms(test109) {"109"}

# RPC server executables.  Each of these is tested (if it exists)
# when running the RPC tests.
set svc_list { berkeley_db_svc berkeley_db_cxxsvc \
    berkeley_db_javasvc berkeley_dbje_svc }
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
	{ scr023	chk.q }
	{ scr024	chk.bdb }
	{ scr025	chk.cxxmulti }
	{ scr026	chk.method }
	{ scr027	chk.javas }
	{ scr028	chk.rtc }
	{ scr029	chk.get }
	{ scr030	chk.build }
}
