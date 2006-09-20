
typedef uint64 TrID; /* our TrID is 6 bytes */

typedef struct st_transaction
{
  TrID           trid, min_read_from, commit_trid;
  struct st_transaction *next, *prev;
  /* Note! if short_id is 0, trx is NOT initialized */
  uint16         short_id;
  LF_PINS        *pins;
} TRX;

#define SHORT_ID_MAX 65535

extern uint trxman_active_transactions, trxman_allocated_transactions;

extern TRX **short_id_to_trx;
extern my_atomic_rwlock_t LOCK_short_id_to_trx;

int trxman_init();
int trxman_end();
TRX *trxman_new_trx();
void trxman_end_trx(TRX *trx, my_bool commit);
#define trxman_commit_trx(T) trxman_end_trx(T, TRUE)
#define trxman_abort_trx(T)  trxman_end_trx(T, FALSE)
void trxman_free_trx(TRX *trx);
my_bool trx_can_read_from(TRX *trx, TrID trid);

