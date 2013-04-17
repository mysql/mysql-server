/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_ROLLBACK_LOG_NODE_CACHE_H
#define TOKU_ROLLBACK_LOG_NODE_CACHE_H

#ident "$Id: rollback.h 49033 2012-10-17 18:48:30Z zardosht $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

class rollback_log_node_cache {
public:
    void init (uint32_t max_num_avail_nodes);
    void destroy();
    // returns true if rollback log node was successfully added,
    // false otherwise
    bool give_rollback_log_node(TOKUTXN txn, ROLLBACK_LOG_NODE log);
    // if a rollback log node is available, will set log to it,
    // otherwise, will set log to NULL and caller is on his own
    // for getting a rollback log node
    void get_rollback_log_node(TOKUTXN txn, ROLLBACK_LOG_NODE* log);

private:
    BLOCKNUM* m_avail_blocknums;
    uint32_t* m_hashes;
    uint32_t m_first;
    uint32_t m_num_avail;
    uint32_t m_max_num_avail;
    toku_mutex_t m_mutex;
};


ENSURE_POD(rollback_log_node_cache);

#endif // TOKU_ROLLBACK_LOG_NODE_CACHE_H
