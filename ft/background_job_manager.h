/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef BACKGROUND_JOB_MANAGER_H
#define BACKGROUND_JOB_MANAGER_H
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


//
// The background job manager keeps track of the existence of 
// background jobs running. We use the background job manager
// to allow threads to perform background jobs on various pieces 
// of the system (e.g. cachefiles and cloned pairs being written out 
// for checkpoint)
//

typedef struct background_job_manager_struct *BACKGROUND_JOB_MANAGER;


void bjm_init(BACKGROUND_JOB_MANAGER* bjm);
void bjm_destroy(BACKGROUND_JOB_MANAGER bjm);

//
// Re-allows a background job manager to accept background jobs
//
void bjm_reset(BACKGROUND_JOB_MANAGER bjm);

//
// add a background job. If return value is 0, then the addition of the job
// was successful and the user may perform the background job. If return
// value is non-zero, then adding of the background job failed and the user
// may not perform the background job.
//
int bjm_add_background_job(BACKGROUND_JOB_MANAGER bjm);

//
// remove a background job
//
void bjm_remove_background_job(BACKGROUND_JOB_MANAGER bjm);

//
// This function waits for all current background jobs to be removed. If the user
// calls bjm_add_background_job while this function is running, or after this function
// has completed, bjm_add_background_job returns an error. 
//
void bjm_wait_for_jobs_to_finish(BACKGROUND_JOB_MANAGER bjm);

#endif
