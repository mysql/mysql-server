#include "db_cxx.h"

Db::~Db() {
    if (!the_db) {
	the_db->close(db, 0);
	the_db->toku_internal = 0;
	// Do we need to clean up "private environments"?
    }
}

int DB::close (u_int32_t flags) {
    if (!the_db) {
	return EINVAL;
    }
    int ret = the_db->close(flags);

    the_db->toku_internal = 0;
    // Do we need to clean up "private environments"?

    return ret;
}
