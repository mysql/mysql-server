#include "ybt.h"
#include "brttypes.h"

int keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);
void test_keycompare (void) ;

int default_compare_fun (DB *, const DBT *, const DBT*);
