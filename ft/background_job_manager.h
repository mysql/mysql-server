/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef BACKGROUND_JOB_MANAGER_H
#define BACKGROUND_JOB_MANAGER_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


typedef struct background_job_manager_struct *BACKGROUND_JOB_MANAGER;

void bjm_init(BACKGROUND_JOB_MANAGER* bjm);
void bjm_destroy(BACKGROUND_JOB_MANAGER bjm);
void bjm_reset(BACKGROUND_JOB_MANAGER bjm);

int bjm_add_background_job(BACKGROUND_JOB_MANAGER bjm);
void bjm_remove_background_job(BACKGROUND_JOB_MANAGER bjm);
void bjm_wait_for_jobs_to_finish(BACKGROUND_JOB_MANAGER bjm);

#endif
