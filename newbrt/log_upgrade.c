/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "log_header.h"
#include "checkpoint.h"

static uint64_t footprint = 0;  // for debug and accountability
static uint64_t footprint_previous_upgrade = 0;  // for debug and accountability

uint64_t
toku_log_upgrade_get_footprint(void) {
    return footprint + (100000 * footprint_previous_upgrade);
}

#define FOOTPRINT(x) footprint=footprint_start+(x*footprint_increment)
#define FOOTPRINTSETUP(increment) uint64_t footprint_start=footprint; uint64_t footprint_increment=increment;

// The lock file is used to detect a failed upgrade.  It is created at the start
// of the upgrade procedure and deleted at the end of the upgrade procedure.  If
// it exists at startup, then there was a crash during an upgrade, and the previous
// upgrade attempt must be undone.
static const char upgrade_lock_file_suffix[]  = "/__tokudb_upgrade_dont_delete_me";
static const char upgrade_commit_file_suffix[]  = "/__tokudb_upgrade_commit_dont_delete_me";

//This will be the base information needed.
//Future 'upgrade in progress' files that need more information
//should store it AFTER the prefix checksum, and have its own checksum.
static const int upgrade_lock_prefix_size = 8  // magic ("tokuupgr")
                                           +4  // version upgrading to
                                           +4  // upgrading from version
                                           +4  // size of suffix (data following prefix checksum)
                                           +4; // prefix checksum


static int 
verify_clean_shutdown_of_log_version_current(const char *log_dir, LSN * last_lsn) {
    int rval = DB_RUNRECOVERY;
    TOKULOGCURSOR logcursor = NULL;
    int r;
    FOOTPRINTSETUP(100);
    
    FOOTPRINT(1);

    r = toku_logcursor_create(&logcursor, log_dir);
    assert(r == 0);
    struct log_entry *le = NULL;
    r = toku_logcursor_last(logcursor, &le);
    if (r == 0) {
	FOOTPRINT(2);
	if (le->cmd==LT_shutdown) {
	    LSN lsn = le->u.shutdown.lsn;
	    if (last_lsn)
		*last_lsn = lsn;
	    rval = 0;
	}
    }
    r = toku_logcursor_destroy(&logcursor);
    assert(r == 0);
    return rval;
}


static int 
verify_clean_shutdown_of_log_version_1(const char *log_dir, LSN * last_lsn) {
    FOOTPRINTSETUP(100);
    
    FOOTPRINT(1);
    //TODO: Remove this hack:
    //Base this function on
    // - (above)verify_clean_shutdown_of_log_version_current
    // - (3.1)tokudb_needs_recovery
    // - do breadth/depth first search to find out which functions have to be copied over from 3.1
    // - Put copied functions in .. backwards_log_1.[ch]
    LSN lsn = {.lsn = 1LLU << 40};
    if (last_lsn)
	*last_lsn = lsn;
    log_dir = log_dir;
    
    return 0;
}


static int
verify_clean_shutdown_of_log_version(const char *log_dir, uint32_t version, LSN *last_lsn) {
    // return 0 if clean shutdown, DB_RUNRECOVERY if not clean shutdown
    // examine logfile at logfilenum and possibly logfilenum-1
    int r = 0;
    FOOTPRINTSETUP(100);

    if (version == TOKU_LOG_VERSION_1)  {
	FOOTPRINT(1);
	r = verify_clean_shutdown_of_log_version_1(log_dir, last_lsn);
    }
    else {
	FOOTPRINT(2);
	assert(version == TOKU_LOG_VERSION);
	r = verify_clean_shutdown_of_log_version_current(log_dir, last_lsn);
    }
    return r;
}
    



//Cross the Rubicon (POINT OF NO RETURN)
static int
convert_logs_and_fsync(const char *log_dir, const char *env_dir, uint32_t from_version, uint32_t to_version) {
    int r;
    FOOTPRINTSETUP(100);

    r = verify_clean_shutdown_of_log_version(log_dir, to_version, NULL);
    assert(r==0);
    r = toku_delete_all_logs_of_version(log_dir, from_version);
    assert(r==0);
    r = toku_fsync_dir_by_name_without_accounting(log_dir);
    assert(r==0);
    if (to_version==TOKU_LOG_VERSION_1) {
        //Undo an upgrade from version 1.
        //Delete rollback cachefile if it exists.
	FOOTPRINT(1);
	
        int rollback_len = strlen(log_dir) + sizeof(ROLLBACK_CACHEFILE_NAME) +1; //1 for '/'
        char rollback_fname[rollback_len];
        
        {
            int l = snprintf(rollback_fname, sizeof(rollback_fname),
                             "%s/%s", env_dir, ROLLBACK_CACHEFILE_NAME);
            assert(l+1 == (signed)(sizeof(rollback_fname)));
        }
        r = unlink(rollback_fname);
        assert(r==0 || errno==ENOENT);
        if (r==0) {
            r = toku_fsync_dir_by_name_without_accounting(env_dir);
            assert(r==0);
        }
    }
    return r;
}

//After this function completes:
//  If any log files exist they are all of the same version.
//  There is no lock file.
//  There is no commit file.
static int
cleanup_previous_upgrade_attempt(const char *env_dir, const char *log_dir,
                                 const char *upgrade_lock_fname,
                                 const char *upgrade_commit_fname) {
    int r = 0;
    int lock_fd;
    int commit_fd;
    unsigned char prefix[upgrade_lock_prefix_size];
    FOOTPRINTSETUP(1000);

    commit_fd = open(upgrade_commit_fname, O_RDONLY|O_BINARY, S_IRWXU);
    if (commit_fd<0) {
        assert(errno==ENOENT);
    }
    lock_fd = open(upgrade_lock_fname, O_RDONLY|O_BINARY, S_IRWXU);
    if (lock_fd<0) {
        assert(errno == ENOENT);
        //Nothing to clean up (lock file does not exist).
    }
    else { //Lock file exists.  Will commit or abort the upgrade.
	FOOTPRINT(1);
        int64_t n = pread(lock_fd, prefix, upgrade_lock_prefix_size, 0);
        assert(n>=0 && n <= upgrade_lock_prefix_size);
        struct rbuf rb;
        rb.size  = upgrade_lock_prefix_size;
        rb.buf   = prefix;
        rb.ndone = 0;
        if (n == upgrade_lock_prefix_size) {
	    FOOTPRINT(2);
            //Check magic number
            bytevec magic;
            rbuf_literal_bytes(&rb, &magic, 8);
            assert(memcmp(magic,"tokuupgr",8)==0);
            uint32_t to_version       = rbuf_network_int(&rb);
            uint32_t from_version     = rbuf_network_int(&rb);
            uint32_t suffix_length    = rbuf_int(&rb);
            uint32_t stored_x1764     = rbuf_int(&rb);
            uint32_t calculated_x1764 = x1764_memory(rb.buf, rb.size-4);
            assert(calculated_x1764 == stored_x1764);
            //Now that checksum matches, verify data.

            assert(to_version == TOKU_LOG_VERSION); //Only upgrading directly to newest log version.
            assert(from_version < TOKU_LOG_VERSION);  //Otherwise it isn't an upgrade.
            assert(from_version >= TOKU_LOG_MIN_SUPPORTED_VERSION);  //TODO: make this an error case once we have 3 log versions
            assert(suffix_length == 0); //TODO: Future versions may change this.
            if (commit_fd>=0) { //Commit the upgrade
		footprint_previous_upgrade = 1;
		FOOTPRINT(3);
                r = convert_logs_and_fsync(log_dir, env_dir, from_version, to_version);
                assert(r==0);
            }
            else { //Abort the upgrade
		footprint_previous_upgrade = 2;
		FOOTPRINT(4);
                r = convert_logs_and_fsync(log_dir, env_dir, to_version, from_version);
                assert(r==0);
            }
        }
        else { // We never finished writing lock file: commit file cannot exist yet.
            // We are aborting the upgrade, but because the previous attempt never got past
	    // writing the lock file, nothing needs to be undone.
            assert(commit_fd<0);
        }
        { //delete lock file
            r = close(lock_fd);
            assert(r==0);
            r = unlink(upgrade_lock_fname);
            assert(r==0);
            r = toku_fsync_dir_by_name_without_accounting(log_dir);
            assert(r==0);
        }
    }
    if (commit_fd>=0) { //delete commit file
        r = close(commit_fd);
        assert(r==0);
        r = unlink(upgrade_commit_fname);
        assert(r==0);
        r = toku_fsync_dir_by_name_without_accounting(log_dir);
        assert(r==0);
    }
    return r;
}


static int
write_commit_file_and_fsync(const char *log_dir, const char * upgrade_commit_fname) {
    int fd;
    fd = open(upgrade_commit_fname, O_RDWR|O_BINARY|O_CREAT|O_EXCL, S_IRWXU);
    assert(fd>=0);

    int r;
    r = toku_file_fsync_without_accounting(fd);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    r = toku_fsync_dir_by_name_without_accounting(log_dir);
    assert(r==0);
    return r;
}

static int
write_lock_file_and_fsync(const char *log_dir, const char * upgrade_lock_fname, uint32_t from_version) {
    int fd;
    fd = open(upgrade_lock_fname, O_RDWR|O_BINARY|O_CREAT|O_EXCL, S_IRWXU);
    assert(fd>=0);

    char buf[upgrade_lock_prefix_size];
    struct wbuf wb;
    const int suffix_size = 0;
    wbuf_init(&wb, buf, upgrade_lock_prefix_size);
    { //Serialize to wbuf
        wbuf_literal_bytes(&wb, "tokuupgr", 8);  //magic
        wbuf_network_int(&wb, TOKU_LOG_VERSION); //to version
        wbuf_network_int(&wb, from_version);     //from version
        wbuf_int(&wb, suffix_size);              //Suffix Length
        u_int32_t checksum = x1764_finish(&wb.checksum);
        wbuf_int(&wb, checksum);                 //checksum
        assert(wb.ndone == wb.size);
    }
    toku_os_full_pwrite(fd, wb.buf, wb.size, 0);
    {
        //Serialize suffix to wbuf and then disk (if exist)
        //There is no suffix as of TOKU_LOG_VERSION_2
    }
    int r;
    r = toku_file_fsync_without_accounting(fd);
    assert(r==0);
    r = close(fd);
    assert(r==0);
    r = toku_fsync_dir_by_name_without_accounting(log_dir);
    assert(r==0);
    return r;
}

// from_version is version of lognumber_newest, which contains last_lsn
static int
upgrade_log(const char *env_dir, const char *log_dir,
            const char * upgrade_lock_fname, const char * upgrade_commit_fname,
            LSN last_lsn,
            uint32_t from_version) { // the real deal
    int r;
    FOOTPRINTSETUP(1000);
    
    r = write_lock_file_and_fsync(log_dir, upgrade_lock_fname, from_version);
    assert(r==0);

    LSN initial_lsn = last_lsn;
    initial_lsn.lsn++;
    CACHETABLE ct;
    TOKULOGGER logger;
    { //Create temporary environment
        r = toku_create_cachetable(&ct, 1<<25, initial_lsn, NULL);
        assert(r == 0);
        toku_cachetable_set_env_dir(ct, env_dir);
        r = toku_logger_create(&logger);
        assert(r == 0);
        toku_logger_write_log_files(logger, FALSE); //Prevent initial creation of log file
        toku_logger_set_cachetable(logger, ct);
        r = toku_logger_open(log_dir, logger);
        assert(r==0);
        r = toku_logger_restart(logger, initial_lsn); //Turn log writing on and create first log file with initial lsn
        assert(r==0);
	FOOTPRINT(1);
    }
    if (from_version == TOKU_LOG_VERSION_1) {
        { //Create rollback cachefile
            r = toku_logger_open_rollback(logger, ct, TRUE);
            assert(r==0);
        }
        { //Checkpoint
            r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL);
            assert(r == 0);
        }
        { //Close rollback cachefile
            r = toku_logger_close_rollback(logger, FALSE);
            assert(r==0);
        }
	FOOTPRINT(2);
    }
    { //Checkpoint
        r = toku_checkpoint(ct, logger, NULL, NULL, NULL, NULL); //fsyncs log dir
        assert(r == 0);
	FOOTPRINT(3);
    }
    { //Close cachetable and logger
        r = toku_logger_shutdown(logger); 
        assert(r==0);
        r = toku_cachetable_close(&ct);
        assert(r==0);
        r = toku_logger_close(&logger);
        assert(r==0);
	FOOTPRINT(4);
    }
    { //Write commit file
        r = write_commit_file_and_fsync(log_dir, upgrade_commit_fname);
        assert(r==0);
    }
    {   // Cross the Rubicon here:
        // Delete all old logs: POINT OF NO RETURN
        r = convert_logs_and_fsync(log_dir, env_dir, from_version, TOKU_LOG_VERSION);
        assert(r==0);
	FOOTPRINT(5);
    }
    { //Delete upgrade lock file and ensure directory is fsynced
        r = unlink(upgrade_lock_fname);
        assert(r==0);
        r = toku_fsync_dir_by_name_without_accounting(log_dir);
        assert(r==0);
    }
    { //Delete upgrade commit file and ensure directory is fsynced
        r = unlink(upgrade_commit_fname);
        assert(r==0);
        r = toku_fsync_dir_by_name_without_accounting(log_dir);
        assert(r==0);
    }
    FOOTPRINT(6);
    return 0;
}

int
toku_maybe_upgrade_log(const char *env_dir, const char *log_dir) {
    int r;
    int lockfd = -1;
    FOOTPRINTSETUP(10000);

    r = toku_recover_lock(log_dir, &lockfd);
    if (r == 0) {
        assert(log_dir);
        assert(env_dir);
        char upgrade_lock_fname[strlen(log_dir) + sizeof(upgrade_lock_file_suffix)];
        { //Generate full fname
            int l = snprintf(upgrade_lock_fname, sizeof(upgrade_lock_fname),
                             "%s%s", log_dir, upgrade_lock_file_suffix);
            assert(l+1 == (ssize_t)(sizeof(upgrade_lock_fname)));
        }
        char upgrade_commit_fname[strlen(log_dir) + sizeof(upgrade_commit_file_suffix)];
        { //Generate full fname
            int l = snprintf(upgrade_commit_fname, sizeof(upgrade_commit_fname),
                             "%s%s", log_dir, upgrade_commit_file_suffix);
            assert(l+1 == (ssize_t)(sizeof(upgrade_commit_fname)));
        }

        r = cleanup_previous_upgrade_attempt(env_dir, log_dir,
                                             upgrade_lock_fname, upgrade_commit_fname);
        if (r==0) {
            uint32_t version_of_logs_on_disk;
            BOOL found_any_logs;
            r = toku_get_version_of_logs_on_disk(log_dir, &found_any_logs, &version_of_logs_on_disk);
            if (r==0) {
                if (!found_any_logs)
                    r = 0; //No logs means no logs to upgrade.
                else if (version_of_logs_on_disk > TOKU_LOG_VERSION)
                    r = TOKUDB_DICTIONARY_TOO_NEW;
                else if (version_of_logs_on_disk < TOKU_LOG_MIN_SUPPORTED_VERSION)
                    r = TOKUDB_DICTIONARY_TOO_OLD;
                else if (version_of_logs_on_disk == TOKU_LOG_VERSION)
                    r = 0; //Logs are up to date
                else {
		    FOOTPRINT(1);
                    LSN last_lsn;
                    r = verify_clean_shutdown_of_log_version(log_dir, version_of_logs_on_disk, &last_lsn);
                    if (r==0) {
			FOOTPRINT(2);
                        r = upgrade_log(env_dir, log_dir,
                                        upgrade_lock_fname, upgrade_commit_fname,
                                        last_lsn, version_of_logs_on_disk);
                    }
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
    return r;
}

