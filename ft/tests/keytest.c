#ident "$Id$"
#include "test.h"
#include "key.h"

void
toku_test_keycompare (void) {
    assert(toku_keycompare("a",1, "a",1)==0);
    assert(toku_keycompare("aa",2, "a",1)>0);
    assert(toku_keycompare("a",1, "aa",2)<0);
    assert(toku_keycompare("b",1, "aa",2)>0);
    assert(toku_keycompare("aa",2, "b",1)<0);
    assert(toku_keycompare("aaaba",5, "aaaba",5)==0);
    assert(toku_keycompare("aaaba",5, "aaaaa",5)>0);
    assert(toku_keycompare("aaaaa",5, "aaaba",5)<0);
    assert(toku_keycompare("aaaaa",3, "aaaba",3)==0);
    assert(toku_keycompare("\000\000\000\a", 4, "\000\000\000\004", 4)>0);
}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    toku_test_keycompare();
    if (verbose) printf("test ok\n");
    return 0;
}
