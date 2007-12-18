#include "db_cxx.h"

Db::~Db() {
    if (!the_db) {
	close(0); // the user should have called close, but we do it here if not done.
    }
}

int Db::close (u_int32_t flags) {
    if (!the_db) {
	return EINVAL;
    }
    the_db->toku_internal = 0;

    int ret = the_db->close(flags);

    the_db = 0;
    // Do we need to clean up "private environments"?
    // What about cursors?  They should be cleaned up already.

    return ret;
}
