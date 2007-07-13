#ifndef _YDB_CONSTANTS_H
#define _YDB_CONSTANTS_H

enum {
  DB_KEYEMPTY      = -30998,
  DB_KEYEXIST      = -30997,
  DB_LOCK_DEADLOCK = -30996,
  DB_NOTFOUND      = -30991,

  // Private
  DB_BADFORMAT     = -31000
};

enum {
  //DB_AFTER   =  1,
  DB_FIRST     = 10,
  DB_GET_BOTH  = 11,
  DB_LAST      = 18,
  DB_NEXT      = 19,
  DB_NEXT_DUP  = 20,
  DB_PREV      = 27,
  DB_SET       = 30,
  DB_SET_RANGE = 32,
  DB_RMW = 0x40000000
};

#endif
