#if !defined(TOKUTXN_STATE_H)
#define TOKUTXN_STATE_H

// this is a separate file so that the hotindexing tests can see the txn states

enum tokutxn_state {
    TOKUTXN_LIVE,         // initial txn state
    TOKUTXN_PREPARING,    // txn is preparing (or prepared)
    TOKUTXN_COMMITTING,   // txn in the process of committing
    TOKUTXN_ABORTING,     // txn in the process of aborting
    TOKUTXN_RETIRED,      // txn no longer exists
};
typedef enum tokutxn_state TOKUTXN_STATE;

#endif
