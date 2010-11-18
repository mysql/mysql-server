/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "log_header.h"
#include "checkpoint.h"

static uint64_t footprint = 0;  // for debug and accountability

uint64_t
toku_log_upgrade_get_footprint(void) {
    return footprint;
}

// Footprint concept here is that each function increments a different decimal digit.
// The cumulative total shows the path taken for the upgrade.
// Each function must have a single return for this to work.
#define FOOTPRINT(x) function_footprint=(x*footprint_increment)
#define FOOTPRINTSETUP(increment) uint64_t function_footprint = 0; uint64_t footprint_increment=increment;
#define FOOTPRINTCAPTURE footprint+=function_footprint;


static int 
verify_clean_shutdown_of_log_version_current(const char *log_dir, LSN * last_lsn) {
    int rval = TOKUDB_UPGRADE_FAILURE;
    TOKULOGCURSOR cursor = NULL;
    int r;
    FOOTPRINTSETUP(100);
    
    FOOTPRINT(1);

    r = toku_logcursor_create(&cursor, log_dir);
    assert(r == 0);
    struct log_entry *le = NULL;
    r = toku_logcursor_last(cursor, &le);
    if (r == 0) {
	FOOTPRINT(2);
	if (le->cmd==LT_shutdown) {
	    LSN lsn = le->u.shutdown.lsn;
	    if (last_lsn)
		*last_lsn = lsn;
	    rval = 0;
	}
    }
    r = toku_logcursor_destroy(&cursor);
    assert(r == 0);
    FOOTPRINTCAPTURE;
    return rval;
}


static int 
verify_clean_shutdown_of_log_version_old(const char *log_dir, LSN * last_lsn) {
    int rval = TOKUDB_UPGRADE_FAILURE;
    int r;
    FOOTPRINTSETUP(10);
    
    FOOTPRINT(1);

    int n_logfiles;
    char **logfiles;
    r = toku_logger_find_logfiles(log_dir, &logfiles, &n_logfiles);
    if (r!=0) return r;

    char *basename;
    TOKULOGCURSOR cursor;
    struct log_entry *entry;
    //Only look at newest log
    basename = strrchr(logfiles[n_logfiles-1], '/') + 1;
    int version;
    long long index = -1;
    r = sscanf(basename, "log%lld.tokulog%d", &index, &version);
    assert(r==2);  // found index and version
    assert(version>=TOKU_LOG_MIN_SUPPORTED_VERSION);
    assert(version< TOKU_LOG_VERSION); //Must be old
    // find last LSN
    r = toku_logcursor_create_for_file(&cursor, log_dir, basename);
    if (r==0) {
        r = toku_logcursor_last(cursor, &entry);
        if (r == 0) {
            FOOTPRINT(2);
            if (entry->cmd==LT_shutdown) {
                LSN lsn = entry->u.shutdown.lsn;
                if (last_lsn)
                    *last_lsn = lsn;
                rval = 0;
            }
        }
        r = toku_logcursor_destroy(&cursor);
        assert(r == 0);
    }
    for(int i=0;i<n_logfiles;i++) {
        toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    FOOTPRINTCAPTURE;
    return rval;
}


static int
verify_clean_shutdown_of_log_version(const char *log_dir, uint32_t version, LSN *last_lsn) {
    // return 0 if clean shutdown, TOKUDB_UPGRADE_FAILURE if not clean shutdown
    // examine logfile at logfilenum and possibly logfilenum-1
    int r = 0;
    FOOTPRINTSETUP(1000);

    if (version < TOKU_LOG_VERSION)  {
	FOOTPRINT(1);
	r = verify_clean_shutdown_of_log_version_old(log_dir, last_lsn);
    }
    else {
	FOOTPRINT(2);
	assert(version == TOKU_LOG_VERSION);
	r = verify_clean_shutdown_of_log_version_current(log_dir, last_lsn);
    }
    FOOTPRINTCAPTURE;
    return r;
}
    

static int
upgrade_log(const char *env_dir, const char *log_dir, LSN last_lsn) { // the real deal
    int r;
    FOOTPRINTSETUP(10000);
    
    LSN initial_lsn = last_lsn;
    initial_lsn.lsn++;
    CACHETABLE ct;
    TOKULOGGER logger;

    FOOTPRINT(1);

    { //Create temporary environment
        r = toku_create_cachetable(&ct, 1<<25, initial_lsn, NULL);
        assert(r == 0);
        toku_cachetable_set_env_dir(ct, env_dir);
        r = toku_logger_create(&logger);
        assert(r == 0);
        toku_logger_set_cachetable(logger, ct);
        r = toku_logger_open(log_dir, logger);
        assert(r==0);
    }
    { //Checkpoint
        r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL); //fsyncs log dir
        assert(r == 0);
    }
    { //Close cachetable and logger
        r = toku_logger_shutdown(logger); 
        assert(r==0);
        r = toku_cachetable_close(&ct);
        assert(r==0);
        r = toku_logger_close(&logger);
        assert(r==0);
    }
    {
        r = verify_clean_shutdown_of_log_version(log_dir, TOKU_LOG_VERSION, NULL);
        assert(r==0);
    }
    FOOTPRINTCAPTURE;
    return 0;
}


int
toku_maybe_upgrade_log(const char *env_dir, const char *log_dir, LSN * lsn_of_clean_shutdown, BOOL * upgrade_in_progress) {
    int r;
    int lockfd = -1;
    FOOTPRINTSETUP(100000);

    *upgrade_in_progress = FALSE;  // set TRUE only if all criteria are met and we're actually doing an upgrade

    FOOTPRINT(1);
    r = toku_recover_lock(log_dir, &lockfd);
    if (r == 0) {
	FOOTPRINT(2);
        assert(log_dir);
        assert(env_dir);

        uint32_t version_of_logs_on_disk;
        BOOL found_any_logs;
        r = toku_get_version_of_logs_on_disk(log_dir, &found_any_logs, &version_of_logs_on_disk);
        if (r==0) {
	    FOOTPRINT(3);
            if (!found_any_logs)
                r = 0; //No logs means no logs to upgrade.
            else if (version_of_logs_on_disk > TOKU_LOG_VERSION)
                r = TOKUDB_DICTIONARY_TOO_NEW;
            else if (version_of_logs_on_disk < TOKU_LOG_MIN_SUPPORTED_VERSION)
                r = TOKUDB_DICTIONARY_TOO_OLD;
            else if (version_of_logs_on_disk == TOKU_LOG_VERSION)
                r = 0; //Logs are up to date
            else {
                FOOTPRINT(4);
                LSN last_lsn;
                r = verify_clean_shutdown_of_log_version(log_dir, version_of_logs_on_disk, &last_lsn);
                if (r==0) {
                    FOOTPRINT(5);
		    *lsn_of_clean_shutdown = last_lsn;
		    *upgrade_in_progress = TRUE;
                    r = upgrade_log(env_dir, log_dir, last_lsn);
                }
            }
        }
        {
            //Clean up
            int rc;
            rc = toku_recover_unlock(lockfd);
            if (r==0) r = rc;
        }
    }
    FOOTPRINTCAPTURE;
    return r;
}

