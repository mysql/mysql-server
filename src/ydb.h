// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_INTERFACE_H)
#define TOKU_YDB_INTERFACE_H

// Initialize the ydb library globals.  
// Called when the ydb library is loaded.
void toku_ydb_init(void);

// Called when the ydb library is unloaded.
void toku_ydb_destroy(void);

#endif
