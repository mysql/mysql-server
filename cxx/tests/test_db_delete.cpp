#include <stdio.h>
#include <assert.h>
#include <db_cxx.h>

void test_new_delete() {
  Db *db = new Db(0, 0); assert(db != 0);
  delete db;
}

void test_new_open_delete() {
  unlink("test.db");
  Db *db = new Db(0, 0); assert(db != 0);
  int r = db->open(0, "test.db", 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
  r = db->close(0); assert(r == 0);
  delete db;
}

int main() {
  test_new_delete();
  test_new_open_delete();
  return 0;
}
