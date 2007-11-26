#include "ybt.h"
#include "brttypes.h"

int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);
void toku_test_keycompare (void) ;

int toku_default_compare_fun (DB *, const DBT *, const DBT*);
int dont_call_this_compare_fun (DB *, const DBT *, const DBT*);
