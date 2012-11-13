/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */


#ifndef RPL_GTID_H_INCLUDED
#define RPL_GTID_H_INCLUDED


#include <m_string.h>
#include <mysqld_error.h>
#include <my_global.h>
#ifdef MYSQL_SERVER
#include <mysqld.h>
#endif
/**
  Report an error from code that can be linked into either the server
  or mysqlbinlog.  There is no common error reporting mechanism, so we
  have to duplicate the error message (write it out in the source file
  for mysqlbinlog, write it in share/errmsg-utf8.txt for the server).

  @param MYSQLBINLOG_ERROR arguments to mysqlbinlog's 'error'
  function, including the function call parentheses
  @param SERVER_ERROR arguments to my_error, including the function
  call parentheses.
*/
#ifdef MYSQL_CLIENT
#define BINLOG_ERROR(MYSQLBINLOG_ERROR, SERVER_ERROR) error MYSQLBINLOG_ERROR
#else
#define BINLOG_ERROR(MYSQLBINLOG_ERROR, SERVER_ERROR) my_error SERVER_ERROR
#endif


#include "hash.h"
#include "lf.h"
#include "my_atomic.h"

/**
  This macro is used to check that the given character, pointed to by the
  character pointer, is a space or not.
*/
#define SKIP_WHITESPACE() while (my_isspace(&my_charset_utf8_general_ci, *s)) s++
/*
  This macro must be used to filter out parts of the code that
  is not used now but may be useful in future. In other words,
  we want to keep such code until we make up our minds on whether
  it should be removed or not.
*/
#undef NON_DISABLED_GTID

/*
  This macro must be used to filter out parts of the code that
  is not used now but we are not sure if there is a bug around
  them. In other words, we want to keep such code until we have
  time to investigate it.
*/
#undef NON_ERROR_GTID

#ifndef MYSQL_CLIENT
class String;
class THD;
#endif // ifndef MYSQL_CLIENT


/// Type of SIDNO (source ID number, first component of GTID)
typedef int32 rpl_sidno;
/// Type for GNO (group number, second component of GTID)
typedef int64 rpl_gno;
/// Type of binlog_pos (positions in binary log)
typedef int64 rpl_binlog_pos;


/**
  Generic return type for many functions that can succeed or fail.

  This is used in conjuction with the macros below for functions where
  the return status either indicates "success" or "failure".  It
  provides the following features:

   - The macros can be used to conveniently propagate errors from
     called functions back to the caller.

   - If a function is expected to print an error using my_error before
     it returns an error status, then the macros assert that my_error
     has been called.

   - Does a DBUG_PRINT before returning failure.
*/
enum enum_return_status
{
  /// The function completed successfully.
  RETURN_STATUS_OK= 0,
  /// The function completed with error but did not report it.
  RETURN_STATUS_UNREPORTED_ERROR= 1,
  /// The function completed with error and has called my_error.
  RETURN_STATUS_REPORTED_ERROR= 2
};

/**
  Lowest level macro used in the PROPAGATE_* and RETURN_* macros
  below.

  If DBUG_OFF is defined, does nothing. Otherwise, if STATUS is
  RETURN_STATUS_OK, does nothing; otherwise, make a dbug printout and
  (if ALLOW_UNREPORTED==0) assert that STATUS !=
  RETURN_STATUS_UNREPORTED.

  @param STATUS The status to return.
  @param ACTION A text that describes what we are doing: either
  "Returning" or "Propagating" (used in DBUG_PRINT macros)
  @param STATUS_NAME The stringified version of the STATUS (used in
  DBUG_PRINT macros).
  @param ALLOW_UNREPORTED If false, the macro asserts that STATUS is
  not RETURN_STATUS_UNREPORTED_ERROR.
*/
#ifdef DBUG_OFF
#define __CHECK_RETURN_STATUS(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED)
#else
extern void check_return_status(enum_return_status status,
                                const char *action, const char *status_name,
                                int allow_unreported);
#define __CHECK_RETURN_STATUS(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED) \
  check_return_status(STATUS, ACTION, STATUS_NAME, ALLOW_UNREPORTED);
#endif
/**
  Low-level macro that checks if STATUS is RETURN_STATUS_OK; if it is
  not, then RETURN_VALUE is returned.
  @see __DO_RETURN_STATUS
*/
#define __PROPAGATE_ERROR(STATUS, RETURN_VALUE, ALLOW_UNREPORTED)       \
  do                                                                    \
  {                                                                     \
    enum_return_status __propagate_error_status= STATUS;                \
    if (__propagate_error_status != RETURN_STATUS_OK) {                 \
      __CHECK_RETURN_STATUS(__propagate_error_status, "Propagating",    \
                            #STATUS, ALLOW_UNREPORTED);                 \
      DBUG_RETURN(RETURN_VALUE);                                        \
    }                                                                   \
  } while (0)
/// Low-level macro that returns STATUS. @see __DO_RETURN_STATUS
#define __RETURN_STATUS(STATUS, ALLOW_UNREPORTED)                       \
  do                                                                    \
  {                                                                     \
    enum_return_status __return_status_status= STATUS;                  \
    __CHECK_RETURN_STATUS(__return_status_status, "Returning",          \
                          #STATUS, ALLOW_UNREPORTED);                   \
    DBUG_RETURN(__return_status_status);                                \
  } while (0)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise, does a DBUG_PRINT and returns STATUS.
*/
#define PROPAGATE_ERROR(STATUS)                                 \
  __PROPAGATE_ERROR(STATUS, __propagate_error_status, true)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise asserts that STATUS ==
  RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT, and returns STATUS.
*/
#define PROPAGATE_REPORTED_ERROR(STATUS)                        \
  __PROPAGATE_ERROR(STATUS, __propagate_error_status, false)
/**
  If STATUS (of type enum_return_status) returns RETURN_STATUS_OK,
  does nothing; otherwise asserts that STATUS ==
  RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT, and returns 1.
*/
#define PROPAGATE_REPORTED_ERROR_INT(STATUS)    \
  __PROPAGATE_ERROR(STATUS, 1, false)
/**
  If STATUS returns something else than RETURN_STATUS_OK, does a
  DBUG_PRINT.  Then, returns STATUS.
*/
#define RETURN_STATUS(STATUS) __RETURN_STATUS(STATUS, true)
/**
  Asserts that STATUS is not RETURN_STATUS_UNREPORTED_ERROR.  Then, if
  STATUS is RETURN_STATUS_REPORTED_ERROR, does a DBUG_PRINT.  Then,
  returns STATUS.
*/
#define RETURN_REPORTED_STATUS(STATUS) __RETURN_STATUS(STATUS, false)
/// Returns RETURN_STATUS_OK.
#define RETURN_OK DBUG_RETURN(RETURN_STATUS_OK)
/// Does a DBUG_PRINT and returns RETURN_STATUS_REPORTED_ERROR.
#define RETURN_REPORTED_ERROR RETURN_STATUS(RETURN_STATUS_REPORTED_ERROR)
/// Does a DBUG_PRINT and returns RETURN_STATUS_UNREPORTED_ERROR.
#define RETURN_UNREPORTED_ERROR RETURN_STATUS(RETURN_STATUS_UNREPORTED_ERROR)


/// The maximum value of GNO
const rpl_gno MAX_GNO= LONGLONG_MAX;
/// The length of MAX_GNO when printed in decimal.
const int MAX_GNO_TEXT_LENGTH= 19;
/// The maximal possible length of thread_id when printed in decimal.
const int MAX_THREAD_ID_TEXT_LENGTH= 19;


/**
  Parse a GNO from a string.

  @param s Pointer to the string. *s will advance to the end of the
  parsed GNO, if a correct GNO is found.
  @retval GNO if a correct GNO (i.e., 0 or positive number) was found.
  @retval -1 otherwise.
*/
rpl_gno parse_gno(const char **s);
/**
  Formats a GNO as a string.

  @param s The buffer.
  @param gno The GNO.
  @return Length of the generated string.
*/
int format_gno(char *s, rpl_gno gno);


/**
  Represents a UUID.

  This is a POD.  It has to be a POD because it is a member of
  Sid_map::Node which is stored in both HASH and DYNAMIC_ARRAY.
*/
struct Uuid
{
  /**
    Stores the UUID represented by a string on the form
    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX in this object.
    @return RETURN_STATUS_OK or RETURN_STATUS_UNREPORTED_ERROR.
  */
  enum_return_status parse(const char *string);
  /// Set to all zeros.
  void clear() { memset(bytes, 0, BYTE_LENGTH); }
  /// Copies the given 16-byte data to this UUID.
  void copy_from(const uchar *data) { memcpy(bytes, data, BYTE_LENGTH); }
  /// Copies the given UUID object to this UUID.
  void copy_from(const Uuid &data) { copy_from(data.bytes); }
  /// Copies the given UUID object to this UUID.
  void copy_to(uchar *data) const { memcpy(data, bytes, BYTE_LENGTH); }
  /// Returns true if this UUID is equal the given UUID.
  bool equals(const Uuid &other) const
  { return memcmp(bytes, other.bytes, BYTE_LENGTH) == 0; }
  /**
    Generates a 36+1 character long representation of this UUID object
    in the given string buffer.

    @retval 36 - the length of the resulting string.
  */
  size_t to_string(char *buf) const;
  /// Convert the given binary buffer to a UUID
  static size_t to_string(const uchar* bytes_arg, char *buf);
#ifndef DBUG_OFF
  /// Debugging only: Print this Uuid to stdout.
  void print() const
  {
    char buf[TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
#endif
  /// Print this Uuid to the trace file if debug is enabled; no-op otherwise.
  void dbug_print(const char *text= "") const
  {
#ifndef DBUG_OFF
    char buf[TEXT_LENGTH + 1];
    to_string(buf);
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", buf));
#endif
  }
  /**
    Return true if parse() would return succeed, but don't actually
    store the result anywhere.
  */
  static bool is_valid(const char *string);
  /// The number of bytes in the textual representation of a Uuid.
  static const size_t TEXT_LENGTH= 36;
  /// The number of bytes in the data of a Uuid.
  static const size_t BYTE_LENGTH= 16;
  /// The number of bits in the data of a Uuid.
  static const size_t BIT_LENGTH= 128;
  /// The data for this Uuid.
  uchar bytes[BYTE_LENGTH];
private:
  static const int NUMBER_OF_SECTIONS= 5;
  static const int bytes_per_section[NUMBER_OF_SECTIONS];
  static const int hex_to_byte[256];
};


typedef Uuid rpl_sid;


/**
  This has the functionality of mysql_rwlock_t, with two differences:
  1. It has additional operations to check if the read and/or write lock
     is held at the moment.
  2. It is wrapped in an object-oriented interface.

  Note that the assertions do not check whether *this* thread has
  taken the lock (that would be more complicated as it would require a
  dynamic data structure).  Luckily, it is still likely that the
  assertions find bugs where a thread forgot to take a lock, because
  most of the time most locks are only used by one thread at a time.

  The assertions are no-ops when DBUG is off.
*/
class Checkable_rwlock
{
public:
  /// Initialize this Checkable_rwlock.
  Checkable_rwlock()
  {
#ifndef DBUG_OFF
    my_atomic_rwlock_init(&atomic_lock);
    lock_state= 0;
#else
    is_write_lock= false;
#endif
#ifdef MYSQL_SERVER
    mysql_rwlock_init(key_rwlock_global_sid_lock, &rwlock);
#else
    mysql_rwlock_init(0, &rwlock);
#endif
  }
  /// Destroy this Checkable_lock.
  ~Checkable_rwlock()
  {
#ifndef DBUG_OFF
    my_atomic_rwlock_destroy(&atomic_lock);
#endif
    mysql_rwlock_destroy(&rwlock);
  }

  /// Acquire the read lock.
  inline void rdlock()
  {
    mysql_rwlock_rdlock(&rwlock);
    assert_no_wrlock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    my_atomic_add32(&lock_state, 1);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
  }
  /// Acquire the write lock.
  inline void wrlock()
  {
    mysql_rwlock_wrlock(&rwlock);
    assert_no_lock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    my_atomic_store32(&lock_state, -1);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#else
    is_write_lock= true;
#endif
  }
  /// Release the lock (whether it is a write or read lock).
  inline void unlock()
  {
    assert_some_lock();
#ifndef DBUG_OFF
    my_atomic_rwlock_wrlock(&atomic_lock);
    int val= my_atomic_load32(&lock_state);
    if (val > 0)
      my_atomic_add32(&lock_state, -1);
    else if (val == -1)
      my_atomic_store32(&lock_state, 0);
    else
      DBUG_ASSERT(0);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#else
    is_write_lock= false;
#endif
    mysql_rwlock_unlock(&rwlock);
  }
  /**
    Return true if the write lock is held. Must only be called by
    threads that hold a lock.
  */
  inline bool is_wrlock()
  {
    assert_some_lock();
#ifndef DBUG_OFF
    return get_state() == -1;
#else
    return is_write_lock;
#endif
  }

  /// Assert that some thread holds either the read or the write lock.
  inline void assert_some_lock() const
  { DBUG_ASSERT(get_state() != 0); }
  /// Assert that some thread holds the read lock.
  inline void assert_some_rdlock() const
  { DBUG_ASSERT(get_state() > 0); }
  /// Assert that some thread holds the write lock.
  inline void assert_some_wrlock() const
  { DBUG_ASSERT(get_state() == -1); }
  /// Assert that no thread holds the write lock.
  inline void assert_no_wrlock() const
  { DBUG_ASSERT(get_state() >= 0); }
  /// Assert that no thread holds the read lock.
  inline void assert_no_rdlock() const
  { DBUG_ASSERT(get_state() <= 0); }
  /// Assert that no thread holds read or write lock.
  inline void assert_no_lock() const
  { DBUG_ASSERT(get_state() == 0); }

private:
#ifndef DBUG_OFF
  /**
    The state of the lock:
    0 - not locked
    -1 - write locked
    >0 - read locked by that many threads
  */
  volatile int32 lock_state;
  /// Lock to protect my_atomic_* operations on lock_state.
  mutable my_atomic_rwlock_t atomic_lock;
  /// Read lock_state atomically and return the value.
  inline int32 get_state() const
  {
    int32 ret;
    my_atomic_rwlock_rdlock(&atomic_lock);
    ret= my_atomic_load32(const_cast<volatile int32*>(&lock_state));
    my_atomic_rwlock_rdunlock(&atomic_lock);
    return ret;
  }
#else
  bool is_write_lock;
#endif
  /// The rwlock.
  mysql_rwlock_t rwlock;
};


/// Protects Gtid_state.  See comment above gtid_state for details.
extern Checkable_rwlock *global_sid_lock;


/**
  Represents a bidirectional map between SID and SIDNO.

  SIDNOs are always numbers greater or equal to 1.

  This data structure OPTIONALLY knows of a read-write lock that
  protects the number of SIDNOs.  The lock is provided by the invoker
  of the constructor and it is generally the caller's responsibility
  to acquire the read lock.  If the lock is not NULL, access methods
  assert that the caller already holds the read (or write) lock.  If
  the lock is not NULL and a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Sid_map
{
public:
  /**
    Create this Sid_map.

    @param sid_lock Read-write lock that protects updates to the
    number of SIDNOs.
  */
  Sid_map(Checkable_rwlock *sid_lock);
  /// Destroy this Sid_map.
  ~Sid_map();
#ifdef NON_DISABLED_GTID
  /**
    Clears this Sid_map (for RESET MASTER)

    @return RETURN_STATUS_OK or RETURN_STAUTS_REPORTED_ERROR
  */
  enum_return_status clear();
#endif
  /**
    Add the given SID to this map if it does not already exist.

    The caller must hold the read lock or write lock on sid_lock
    before invoking this function.  If the SID does not exist in this
    map, it will release the read lock, take a write lock, update the
    map, release the write lock, and take the read lock again.

    @param sid The SID.
    @retval SIDNO The SIDNO for the SID (a new SIDNO if the SID did
    not exist, an existing if it did exist).
    @retval negative Error. This function calls my_error.
  */
  rpl_sidno add_sid(const rpl_sid &sid);
  /**
    Get the SIDNO for a given SID

    The caller must hold the read lock on sid_lock before invoking
    this function.

    @param sid The SID.
    @retval SIDNO if the given SID exists in this map.
    @retval 0 if the given SID does not exist in this map.
  */
  rpl_sidno sid_to_sidno(const rpl_sid &sid) const
  {
    if (sid_lock != NULL)
      sid_lock->assert_some_lock();
    Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid.bytes,
                                       rpl_sid::BYTE_LENGTH);
    if (node == NULL)
      return 0;
    return node->sidno;
  }
  /**
    Get the SID for a given SIDNO.

    An assertion is raised if the caller does not hold a lock on
    sid_lock, or if the SIDNO is not valid.

    @param sidno The SIDNO.
    @retval NULL The SIDNO does not exist in this map.
    @retval pointer Pointer to the SID.  The data is shared with this
    Sid_map, so should not be modified.  It is safe to read the data
    even after this Sid_map is modified, but not if this Sid_map is
    destroyed.
  */
  const rpl_sid &sidno_to_sid(rpl_sidno sidno) const
  {
    if (sid_lock != NULL)
      sid_lock->assert_some_lock();
    DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
    return (*dynamic_element(&_sidno_to_sid, sidno - 1, Node **))->sid;
  }
  /**
    Return the n'th smallest sidno, in the order of the SID's UUID.

    The caller must hold the read or write lock on sid_lock before
    invoking this function.

    @param n A number in the interval [0, get_max_sidno()-1], inclusively.
  */
  rpl_sidno get_sorted_sidno(rpl_sidno n) const
  {
    if (sid_lock != NULL)
      sid_lock->assert_some_lock();
    rpl_sidno ret= *dynamic_element(&_sorted, n, rpl_sidno *);
    return ret;
  }
  /**
    Return the biggest sidno in this Sid_map.

    The caller must hold the read or write lock on sid_lock before
    invoking this function.
  */
  rpl_sidno get_max_sidno() const
  {
    if (sid_lock != NULL)
      sid_lock->assert_some_lock();
    return _sidno_to_sid.elements;
  }

private:
  /// Node pointed to by both the hash and the array.
  struct Node
  {
    rpl_sidno sidno;
    rpl_sid sid;
  };

  /**
    Create a Node from the given SIDNO and SID and add it to
    _sidno_to_sid, _sid_to_sidno, and _sorted.

    The caller must hold the write lock on sid_lock before invoking
    this function.

    @param sidno The SIDNO to add.
    @param sid The SID to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_node(rpl_sidno sidno, const rpl_sid &sid);

  /// Read-write lock that protects updates to the number of SIDNOs.
  mutable Checkable_rwlock *sid_lock;

  /**
    Array that maps SIDNO to SID; the element at index N points to a
    Node with SIDNO N-1.
  */
  DYNAMIC_ARRAY _sidno_to_sid;
  /**
    Hash that maps SID to SIDNO.  The keys in this array are of type
    rpl_sid.
  */
  HASH _sid_to_sidno;
  /**
    Array that maps numbers in the interval [0, get_max_sidno()-1] to
    SIDNOs, in order of increasing SID.

    @see Sid_map::get_sorted_sidno.
  */
  DYNAMIC_ARRAY _sorted;
};


extern Sid_map *global_sid_map;


/**
  Represents a growable array where each element contains a mutex and
  a condition variable.

  Each element can be locked, unlocked, broadcast, or waited for, and
  it is possible to call "THD::enter_cond" for the condition.  The
  allowed indexes range from 0, inclusive, to get_max_index(),
  inclusive.  Initially there are zero elements (and get_max_index()
  returns -1); more elements can be allocated by calling
  ensure_index().

  This data structure has a read-write lock that protects the number
  of elements.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  elements, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Mutex_cond_array
{
public:
  /**
    Create a new Mutex_cond_array.

    @param global_lock Read-write lock that protects updates to the
    number of elements.
  */
  Mutex_cond_array(Checkable_rwlock *global_lock);
  /// Destroy this object.
  ~Mutex_cond_array();
  /// Lock the n'th mutex.
  inline void lock(int n) const
  {
    assert_not_owner(n);
    mysql_mutex_lock(&get_mutex_cond(n)->mutex);
  }
  /// Unlock the n'th mutex.
  inline void unlock(int n) const
  {
    assert_owner(n);
    mysql_mutex_unlock(&get_mutex_cond(n)->mutex);
  }
  /// Broadcast the n'th condition.
  inline void broadcast(int n) const
  {
    mysql_cond_broadcast(&get_mutex_cond(n)->cond);
  }
  /**
    Assert that this thread owns the n'th mutex.
    This is a no-op if DBUG_OFF is on.
  */
  inline void assert_owner(int n) const
  {
#ifndef DBUG_OFF
    mysql_mutex_assert_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /**
    Assert that this thread does not own the n'th mutex.
    This is a no-op if DBUG_OFF is on.
  */
  inline void assert_not_owner(int n) const
  {
#ifndef DBUG_OFF
    mysql_mutex_assert_not_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /**
    Wait for signal on the n'th condition variable.

    The caller must hold the read lock or write lock on sid_lock, as
    well as the nth mutex lock, before invoking this function.  The
    sid_lock will be released, whereas the mutex will be released
    during the wait and (atomically) re-acquired when the wait ends.
  */
  inline void wait(int n) const
  {
    DBUG_ENTER("Mutex_cond_array::wait");
    Mutex_cond *mutex_cond= get_mutex_cond(n);
    global_lock->unlock();
    mysql_mutex_assert_owner(&mutex_cond->mutex);
    mysql_cond_wait(&mutex_cond->cond, &mutex_cond->mutex);
    mysql_mutex_assert_owner(&mutex_cond->mutex);
    DBUG_VOID_RETURN;
  }
#ifndef MYSQL_CLIENT
  /// Execute THD::enter_cond for the n'th condition variable.
  void enter_cond(THD *thd, int n, PSI_stage_info *stage,
                  PSI_stage_info *old_stage) const;
#endif // ifndef MYSQL_CLIENT
  /// Return the greatest addressable index in this Mutex_cond_array.
  inline int get_max_index() const
  {
    global_lock->assert_some_lock();
    return array.elements - 1;
  }
  /**
    Grows the array so that the given index fits.

    If the array is grown, the global_lock is temporarily upgraded to
    a write lock and then degraded again; there will be a
    short period when the lock is not held at all.

    @param n The index.
    @return RETURN_OK or RETURN_REPORTED_ERROR
  */
  enum_return_status ensure_index(int n);
private:
  /// A mutex/cond pair.
  struct Mutex_cond
  {
    mysql_mutex_t mutex;
    mysql_cond_t cond;
  };
  /// Return the Nth Mutex_cond object
  inline Mutex_cond *get_mutex_cond(int n) const
  {
    global_lock->assert_some_lock();
    DBUG_ASSERT(n <= get_max_index());
    Mutex_cond *ret= *dynamic_element(&array, n, Mutex_cond **);
    DBUG_ASSERT(ret);
    return ret;
  }
  /// Read-write lock that protects updates to the number of elements.
  mutable Checkable_rwlock *global_lock;
  DYNAMIC_ARRAY array;
};


/**
  Holds information about a GTID: the sidno and the gno.

  This is a POD. It has to be a POD because it is part of
  Gtid_specification, which has to be a POD because it is used in
  THD::variables.
*/
struct Gtid
{
  /// SIDNO of this Gtid.
  rpl_sidno sidno;
  /// GNO of this Gtid.
  rpl_gno gno;

  /// Set both components to 0.
  void clear() { sidno= 0; gno= 0; }
  /**
    The maximal length of the textual representation of a SID, not
    including the terminating '\0'.
  */
  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  /**
    Return true if parse() would succeed, but don't store the
    result anywhere.
  */
  static bool is_valid(const char *text);
  /**
    Convert a Gtid to a string.
    @param sid the sid to use. This overrides the sidno of this Gtid.
    @param[out] buf Buffer to store the Gtid in (normally
    MAX_TEXT_LENGTH+1 bytes long).
    @return Length of the string, not counting '\0'.
  */
  int to_string(const rpl_sid &sid, char *buf) const;
  /**
    Convert this Gtid to a string.
    @param sid_map sid_map to use when converting sidno to a SID.
    @param[out] buf Buffer to store the Gtid in (normally
    MAX_TEXT_LENGTH+1 bytes long).
    @return Length of the string, not counting '\0'.
  */
  int to_string(const Sid_map *sid_map, char *buf) const;
  /// Returns true if this Gtid has the same sid and gno as 'other'.
  bool equals(const Gtid &other) const
  { return sidno == other.sidno && gno == other.gno; }
  /**
    Parses the given string and stores in this Gtid.

    @param text The text to parse
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status parse(Sid_map *sid_map, const char *text);

#ifndef DBUG_OFF
  /// Debug only: print this Gtid to stdout.
  void print(const Sid_map *sid_map) const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(sid_map, buf);
    printf("%s\n", buf);
  }
#endif
  /// Print this Gtid to the trace file if debug is enabled; no-op otherwise.
  void dbug_print(const Sid_map *sid_map, const char *text= "") const
  {
#ifndef DBUG_OFF
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(sid_map, buf);
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", buf));
#endif
  }
};


/**
  Represents a set of GTIDs.

  This is structured as an array, indexed by SIDNO, where each element
  contains a linked list of intervals.

  This data structure OPTIONALLY knows of a Sid_map that gives a
  correspondence between SIDNO and SID.  If the Sid_map is NULL, then
  operations that require a Sid_map - printing and parsing - raise an
  assertion.

  This data structure OPTIONALLY knows of a read-write lock that
  protects the number of SIDNOs.  The lock is provided by the invoker
  of the constructor and it is generally the caller's responsibility
  to acquire the read lock.  If the lock is not NULL, access methods
  assert that the caller already holds the read (or write) lock.  If
  the lock is not NULL and a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Gtid_set
{
public:
  /**
    Constructs a new, empty Gtid_set.

    @param sid_map The Sid_map to use, or NULL if this Gtid_set
    should not have a Sid_map.
    @param sid_lock Read-write lock that protects updates to the
    number of SIDs. This may be NULL if such changes do not need to be
    protected.
  */
  Gtid_set(Sid_map *sid_map, Checkable_rwlock *sid_lock= NULL);
  /**
    Constructs a new Gtid_set that contains the groups in the given string, in the same format as add_gtid_text(char *).

    @param sid_map The Sid_map to use for SIDs.
    @param text The text to parse.
    @param status Will be set GS_SUCCESS or GS_ERROR_PARSE or
    GS_ERROR_OUT_OF_MEMORY.
    @param sid_lock Read/write lock to protect changes in the number
    of SIDs with. This may be NULL if such changes do not need to be
    protected.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If the array is grown, sid_lock is
    temporarily upgraded to a write lock and then degraded again;
    there will be a short period when the lock is not held at all.
  */
  Gtid_set(Sid_map *sid_map, const char *text, enum_return_status *status,
           Checkable_rwlock *sid_lock= NULL);
private:
  /// Worker for the constructor.
  void init();
public:
  /// Destroy this Gtid_set.
  ~Gtid_set();
  /**
    Removes all groups from this Gtid_set.

    This does not deallocate anything: if groups are added later,
    existing allocated memory will be re-used.
  */
  void clear();
  /**
    Adds the given GTID to this Gtid_set.

    The SIDNO must exist in the Gtid_set before this function is called.

    @param sidno SIDNO of the group to add.
    @param gno GNO of the group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status _add_gtid(rpl_sidno sidno, rpl_gno gno)
  {
    DBUG_ENTER("Gtid_set::_add_gtid(sidno, gno)");
    Interval_iterator ivit(this, sidno);
    Free_intervals_lock lock(this);
    enum_return_status ret= add_gno_interval(&ivit, gno, gno + 1, &lock);
    DBUG_RETURN(ret);
  }
  /**
    Removes the given GTID from this Gtid_set.

    @param sidno SIDNO of the group to remove.
    @param gno GNO of the group to remove.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status _remove_gtid(rpl_sidno sidno, rpl_gno gno)
  {
    DBUG_ENTER("Gtid_set::_remove_gtid(rpl_sidno, rpl_gno)");
    if (sidno <= get_max_sidno())
    {
      Interval_iterator ivit(this, sidno);
      Free_intervals_lock lock(this);
      enum_return_status ret= remove_gno_interval(&ivit, gno, gno + 1, &lock);
      DBUG_RETURN(ret);
    }
    RETURN_OK;
  }
  /**
    Adds the given GTID to this Gtid_set.

    The SIDNO must exist in the Gtid_set before this function is called.

    @param gtid Gtid to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status _add_gtid(const Gtid &gtid)
  { return _add_gtid(gtid.sidno, gtid.gno); }
  /**
    Adds all groups from the given Gtid_set to this Gtid_set.

    If sid_lock != NULL, then the read lock must be held before
    calling this function. If a new sidno is added so that the array
    of lists of intervals is grown, sid_lock is temporarily upgraded
    to a write lock and then degraded again; there will be a short
    period when the lock is not held at all.

    @param other The Gtid_set to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_gtid_set(const Gtid_set *other);
  /**
    Removes all groups in the given Gtid_set from this Gtid_set.

    @param other The Gtid_set to remove.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove_gtid_set(const Gtid_set *other);
  /**
    Adds the set of GTIDs represented by the given string to this Gtid_set.

    The string must have the format of a comma-separated list of zero
    or more of the following:

       XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX(:NUMBER+(-NUMBER)?)*
       | ANONYMOUS

       Each X is a hexadecimal digit (upper- or lowercase).
       NUMBER is a decimal, 0xhex, or 0oct number.

       The start of an interval must be greater than 0. The end of an
       interval may be 0, but any interval that has an endpoint that
       is smaller than the start is discarded.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If a new sidno is added so that the
    array of lists of intervals is grown, sid_lock is temporarily
    upgraded to a write lock and then degraded again; there will be a
    short period when the lock is not held at all.

    @param text The string to parse.
    @param anonymous[in,out] If this is NULL, ANONYMOUS is not
    allowed.  If this is not NULL, it will be set to true if the
    anonymous group was found; false otherwise.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_gtid_text(const char *text, bool *anonymous= NULL);
  /**
    Decodes a Gtid_set from the given string.

    @param string The string to parse.
    @param length The number of bytes.
    @return GS_SUCCESS or GS_ERROR_PARSE or GS_ERROR_OUT_OF_MEMORY
  */
  enum_return_status add_gtid_encoding(const uchar *encoded, size_t length);
  /// Return true iff the given GTID exists in this set.
  bool contains_gtid(rpl_sidno sidno, rpl_gno gno) const;
  /// Return true iff the given GTID exists in this set.
  bool contains_gtid(const Gtid &gtid) const
  { return contains_gtid(gtid.sidno, gtid.gno); }
  /// Returns the maximal sidno that this Gtid_set currently has space for.
  rpl_sidno get_max_sidno() const
  {
    if (sid_lock)
      sid_lock->assert_some_lock();
    return intervals.elements;
  }
  /**
    Allocates space for all sidnos up to the given sidno in the array of intervals.
    The sidno must exist in the Sid_map associated with this Gtid_set.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If the array is grown, sid_lock is
    temporarily upgraded to a write lock and then degraded again;
    there will be a short period when the lock is not held at all.

    @param sidno The SIDNO.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno(rpl_sidno sidno);
  /// Returns true if this Gtid_set is a subset of the other Gtid_set.
  bool is_subset(const Gtid_set *super) const;
  /// Returns true if there is a least one element of this Gtid_set in
  /// the other Gtid_set.
  bool is_intersection(const Gtid_set *other) const;
  /// Result is the intersection of this Gtid_set and the other Gtid_set.
  enum_return_status intersection(const Gtid_set *other, Gtid_set *result);
  /// Returns true if this Gtid_set is empty.
  bool is_empty() const
  {
    Gtid_iterator git(this);
    return git.get().sidno == 0;
  }
  /**
    Returns true if this Gtid_set contains at least one GTID with
    the given SIDNO.

    @param sidno The SIDNO to test.
    @retval true The SIDNO is less than or equal to the max SIDNO, and
    there is at least one group with this SIDNO.
    @retval false The SIDNO is greater than the max SIDNO, or there is
    no group with this SIDNO.
  */
  bool contains_sidno(rpl_sidno sidno) const
  {
    DBUG_ASSERT(sidno >= 1);
    if (sidno > get_max_sidno())
      return false;
    Const_interval_iterator ivit(this, sidno);
    return ivit.get() != NULL;
  }
  /**
    Returns true if the given string is a valid specification of a
    Gtid_set, false otherwise.
  */
  static bool is_valid(const char *text);
  /**
    Return a newly allocated string containing this Gtid_set, or NULL
    on out of memory.
  */
  char *to_string() const
  {
    char *str= (char *)my_malloc(get_string_length() + 1, MYF(MY_WME));
    if (str != NULL)
      to_string(str);
    return str;
  }
#ifndef DBUG_OFF
  /// Debug only: Print this Gtid_set to stdout.
  void print() const
  {
    char *str= to_string();
    printf("%s\n", str);
    my_free(str);
  }
#endif
  /**
    Print this Gtid_set to the trace file if debug is enabled; no-op
    otherwise.
  */
  void dbug_print(const char *text= "") const
  {
#ifndef DBUG_OFF
    char *str= to_string();
    DBUG_PRINT("info", ("%s%s'%s'", text, *text ? ": " : "", str));
    my_free(str);
#endif
  }

  /**
    Class Gtid_set::String_format defines the separators used by
    Gtid_set::to_string.
  */
  struct String_format
  {
    /// The generated string begins with this.
    const char *begin;
    /// The generated string begins with this.
    const char *end;
    /// In 'SID:GNO', this is the ':'
    const char *sid_gno_separator;
    /// In 'SID:GNO-GNO', this is the '-'
    const char *gno_start_end_separator;
    /// In 'SID:GNO:GNO', this is the second ':'
    const char *gno_gno_separator;
    /// In 'SID:GNO,SID:GNO', this is the ','
    const char *gno_sid_separator;
    /// If the set is empty and this is not NULL, then this string is generated.
    const char *empty_set_string;
    /// The following fields are the lengths of each field above.
    const int begin_length;
    const int end_length;
    const int sid_gno_separator_length;
    const int gno_start_end_separator_length;
    const int gno_gno_separator_length;
    const int gno_sid_separator_length;
    const int empty_set_string_length;
  };
  /**
    Returns the length of the output from to_string.

    @warning This does not include the trailing '\0', so your buffer
    needs space for get_string_length() + 1 characters.

    @param string_format String_format object that specifies
    separators in the resulting text.
    @return The length.
  */
  int get_string_length(const String_format *string_format= NULL) const;
  /**
    Formats this Gtid_set as a string and saves in a given buffer.

    @param[out] buf Pointer to the buffer where the string should be
    stored. This should have size at least get_string_length()+1.
    @param string_format String_format object that specifies
    separators in the resulting text.
    @return Length of the generated string.
  */
  int to_string(char *buf, const String_format *string_format= NULL) const;

  /**
    Formats a Gtid_set as a string and saves in a newly allocated buffer.
    @param[out] buf Pointer to pointer to string. The function will
    set it to point to the newly allocated buffer, or NULL on out of memory.
    @param string_format Specifies how to format the string.
    @retval Length of the generated string, or -1 on out of memory.
  */
  int to_string(char **buf, const String_format *string_format= NULL) const;

  /**
    The default String_format: the format understood by
    add_gtid_text(const char *).
  */
  static const String_format default_string_format;
  /**
    String_format useful to generate an SQL string: the string is
    wrapped in single quotes and there is a newline between SIDs.
  */
  static const String_format sql_string_format;
  /**
    String_format for printing the Gtid_set commented: the string is
    not quote-wrapped, and every SID is on a new line with a leading '# '.
  */
  static const String_format commented_string_format;

  /// Return the Sid_map associated with this Gtid_set.
  Sid_map *get_sid_map() const { return sid_map; }

  /**
    Represents one element in the linked list of intervals associated
    with a SIDNO.
  */
  struct Interval
  {
  public:
    /// The first GNO of this interval.
    rpl_gno start;
    /// The first GNO after this interval.
    rpl_gno end;
    /// Return true iff this interval is equal to the given interval.
    bool equals(const Interval &other) const
    {
      return start == other.start && end == other.end;
    }
    /// Pointer to next interval in list.
    Interval *next;
  };

  /**
    Provides an array of Intervals that this Gtid_set can use when
    groups are subsequently added.  This can be used as an
    optimization, to reduce allocation for sets that have a known
    number of intervals.

    @param n_intervals The number of intervals to add.
    @param intervals Array of n_intervals intervals.
  */
  void add_interval_memory(int n_intervals, Interval *intervals_param)
  {
    if (sid_lock != NULL)
      mysql_mutex_lock(&free_intervals_mutex);
    add_interval_memory_lock_taken(n_intervals, intervals_param);
    if (sid_lock != NULL)
      mysql_mutex_unlock(&free_intervals_mutex);
  }


  /**
    Iterator over intervals for a given SIDNO.

    This is an abstract template class, used as a common base class
    for Const_interval_iterator and Interval_iterator.

    The iterator always points to an interval pointer.  The interval
    pointer is either the initial pointer into the list, or the next
    pointer of one of the intervals in the list.
  */
  template<typename Gtid_set_p, typename Interval_p> class Interval_iterator_base
  {
  public:
    /**
      Construct a new iterator over the GNO intervals for a given Gtid_set.

      @param gtid_set The Gtid_set.
      @param sidno The SIDNO.
    */
    Interval_iterator_base(Gtid_set_p gtid_set, rpl_sidno sidno)
    {
      DBUG_ASSERT(sidno >= 1 && sidno <= gtid_set->get_max_sidno());
      init(gtid_set, sidno);
    }
    /// Construct a new iterator over the free intervals of a Gtid_set.
    Interval_iterator_base(Gtid_set_p gtid_set)
    { p= const_cast<Interval_p *>(&gtid_set->free_intervals); }
    /// Reset this iterator.
    inline void init(Gtid_set_p gtid_set, rpl_sidno sidno)
    { p= dynamic_element(&gtid_set->intervals, sidno - 1, Interval_p *); }
    /// Advance current_elem one step.
    inline void next()
    {
      DBUG_ASSERT(*p != NULL);
      p= const_cast<Interval_p *>(&(*p)->next);
    }
    /// Return current_elem.
    inline Interval_p get() const { return *p; }
  protected:
    /**
      Holds the address of the 'next' pointer of the previous element,
      or the address of the initial pointer into the list, if the
      current element is the first element.
    */
    Interval_p *p;
  };

  /**
    Iterator over intervals of a const Gtid_set.
  */
  class Const_interval_iterator
    : public Interval_iterator_base<const Gtid_set *, const Interval *>
  {
  public:
    /// Create this Const_interval_iterator.
    Const_interval_iterator(const Gtid_set *gtid_set, rpl_sidno sidno)
      : Interval_iterator_base<const Gtid_set *, const Interval *>(gtid_set, sidno) {}
    /// Create this Const_interval_iterator.
    Const_interval_iterator(const Gtid_set *gtid_set)
      : Interval_iterator_base<const Gtid_set *, const Interval *>(gtid_set) {}
  };

  /**
    Iterator over intervals of a non-const Gtid_set, with additional
    methods to modify the Gtid_set.
  */
  class Interval_iterator
    : public Interval_iterator_base<Gtid_set *, Interval *>
  {
  public:
    /// Create this Interval_iterator.
    Interval_iterator(Gtid_set *gtid_set, rpl_sidno sidno)
      : Interval_iterator_base<Gtid_set *, Interval *>(gtid_set, sidno) {}
    /// Destroy this Interval_iterator.
    Interval_iterator(Gtid_set *gtid_set)
      : Interval_iterator_base<Gtid_set *, Interval *>(gtid_set) {}
  private:
    /**
      Set current_elem to the given Interval but do not touch the
      next pointer of the given Interval.
    */
    inline void set(Interval *iv) { *p= iv; }
    /// Insert the given element before current_elem.
    inline void insert(Interval *iv) { iv->next= *p; set(iv); }
    /// Remove current_elem.
    inline void remove(Gtid_set *gtid_set)
    {
      DBUG_ASSERT(get() != NULL);
      Interval *next= (*p)->next;
      gtid_set->put_free_interval(*p);
      set(next);
    }
    /**
      Only Gtid_set is allowed to use set/insert/remove.

      They are not safe to use from other code because: (1) very easy
      to make a mistakes (2) they don't clear cached_string_format or
      cached_string_length.
    */
    friend class Gtid_set;
  };


  /**
    Iterator over all groups in a Gtid_set.  This is a const
    iterator; it does not allow modification of the Gtid_set.
  */
  class Gtid_iterator
  {
  public:
    Gtid_iterator(const Gtid_set *gs)
      : gtid_set(gs), sidno(0), ivit(gs)
    {
      if (gs->sid_lock != NULL)
        gs->sid_lock->assert_some_wrlock();
      next_sidno();
    }
    /// Advance to next group.
    inline void next()
    {
      DBUG_ASSERT(gno > 0 && sidno > 0);
      // go to next group in current interval
      gno++;
      // end of interval? then go to next interval for this sidno
      if (gno == ivit.get()->end)
      {
        ivit.next();
        const Interval *iv= ivit.get();
        // last interval for this sidno? then go to next sidno
        if (iv == NULL)
        {
          next_sidno();
          // last sidno? then don't try more
          if (sidno == 0)
            return;
          iv= ivit.get();
        }
        gno= iv->start;
      }
    }
    /// Return next group, or {0,0} if we reached the end.
    inline Gtid get() const
    {
      Gtid ret= { sidno, gno };
      return ret;
    }
  private:
    /// Find the next sidno that has one or more intervals.
    inline void next_sidno()
    {
      const Interval *iv;
      do
      {
        sidno++;
        if (sidno > gtid_set->get_max_sidno())
        {
          sidno= 0;
          gno= 0;
          return;
        }
        ivit.init(gtid_set, sidno);
        iv= ivit.get();
      } while (iv == NULL);
      gno= iv->start;
    }
    /// The Gtid_set we iterate over.
    const Gtid_set *gtid_set;
    /**
      The SIDNO of the current element, or 0 if the iterator is past
      the last element.
    */
    rpl_sidno sidno;
    /**
      The GNO of the current element, or 0 if the iterator is past the
      last element.
    */
    rpl_gno gno;
    /// Iterator over the intervals for the current SIDNO.
    Const_interval_iterator ivit;
  };

public:

  /**
    Encodes this Gtid_set as a binary string.
  */
  void encode(uchar *buf) const;
  /**
    Returns the length of this Gtid_set when encoded using the
    encode() function.
  */
  size_t get_encoded_length() const;

private:
  /**
    Contains a list of intervals allocated by this Gtid_set.  When a
    method of this class needs a new interval and there are no more
    free intervals, a new Interval_chunk is allocated and the
    intervals of it are added to the list of free intervals.
  */
  struct Interval_chunk
  {
    Interval_chunk *next;
    Interval intervals[1];
  };
  /// The default number of intervals in an Interval_chunk.
  static const int CHUNK_GROW_SIZE= 8;

/*
  Functions sidno_equals() and equals() are only used by unitests
*/
#ifdef NON_DISABLED_UNITTEST_GTID
  /**
    Return true if the given sidno of this Gtid_set contains the same
    intervals as the given sidno of the other Gtid_set.

    @param sidno SIDNO to check for this Gtid_set.
    @param other Other Gtid_set
    @param other_sidno SIDNO to check in other.
    @return true if equal, false is not equal.
  */
  bool sidno_equals(rpl_sidno sidno,
                    const Gtid_set *other, rpl_sidno other_sidno) const;
  /// Returns true if this Gtid_set is equal to the other Gtid_set.
  bool equals(const Gtid_set *other) const;
#endif

  /// Return the number of intervals for the given sidno.
  int get_n_intervals(rpl_sidno sidno) const
  {
    Const_interval_iterator ivit(this, sidno);
    int ret= 0;
    while (ivit.get() != NULL)
    {
      ret++;
      ivit.next();
    }
    return ret;
  }
  /// Return the number of intervals in this Gtid_set.
  int get_n_intervals() const
  {
    if (sid_lock != NULL)
      sid_lock->assert_some_wrlock();
    rpl_sidno max_sidno= get_max_sidno();
    int ret= 0;
    for (rpl_sidno sidno= 1; sidno < max_sidno; sidno++)
      ret+= get_n_intervals(sidno);
    return ret;
  }
  /**
    Allocates a new chunk of Intervals and adds them to the list of
    unused intervals.

    @param size The number of intervals in this chunk
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status create_new_chunk(int size);
  /**
    Returns a fresh new Interval object.

    This usually does not require any real allocation, it only pops
    the first interval from the list of free intervals.  If there are
    no free intervals, it calls create_new_chunk.

    @param out The resulting Interval* will be stored here.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_free_interval(Interval **out);
  /**
    Puts the given interval in the list of free intervals.  Does not
    unlink it from its place in any other list.
  */
  void put_free_interval(Interval *iv);
  /**
    Like add_interval_memory, but does not acquire
    free_intervals_mutex.
    @see Gtid_set::add_interval_memory
  */
  void add_interval_memory_lock_taken(int n_ivs, Interval *ivs);

  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /**
    Lock protecting the list of free intervals.  This lock is only
    used if sid_lock is not NULL.
  */
  mysql_mutex_t free_intervals_mutex;
  /**
    Class representing a lock on free_intervals_mutex.

    This is used by the add_* and remove_* functions.  The lock is
    declared by the top-level function and a pointer to the lock is
    passed down to low-level functions. If the low-level function
    decides to access the free intervals list, then it acquires the
    lock.  The lock is then automatically released by the destructor
    when the top-level function returns.

    The lock is not taken if Gtid_set->sid_lock == NULL; such
    Gtid_sets are assumed to be thread-local.
  */
  class Free_intervals_lock
  {
  public:
    /// Create a new lock, but do not acquire it.
    Free_intervals_lock(Gtid_set *_gtid_set)
      : gtid_set(_gtid_set), locked(false) {}
    /// Lock the lock if it is not already locked.
    void lock_if_not_locked()
    {
      if (gtid_set->sid_lock && !locked)
      {
        mysql_mutex_lock(&gtid_set->free_intervals_mutex);
        locked= true;
      }
    }
    /// Lock the lock if it is locked.
    void unlock_if_locked()
    {
      if (gtid_set->sid_lock && locked)
      {
        mysql_mutex_unlock(&gtid_set->free_intervals_mutex);
        locked= false;
      }
    }
    /// Destroy this object and unlock the lock if it is locked.
    ~Free_intervals_lock()
    {
      unlock_if_locked();
    }
  private:
    Gtid_set *gtid_set;
    bool locked;
  };
  void assert_free_intervals_locked()
  {
    if (sid_lock != NULL)
      mysql_mutex_assert_owner(&free_intervals_mutex);
  }

  /**
    Adds the interval (start, end) to the given Interval_iterator.

    This is the lowest-level function that adds groups; this is where
    Interval objects are added, grown, or merged.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the interval that
    contains start and end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @param lock If this function has to add or remove an interval,
    then this lock will be taken unless it is already taken.  This
    mechanism means that the lock will be taken lazily by
    e.g. add_gtid_set() the first time that the list of free intervals
    is accessed, and automatically released when add_gtid_set()
    returns.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_gno_interval(Interval_iterator *ivitp,
                                      rpl_gno start, rpl_gno end,
                                      Free_intervals_lock *lock);
  /**
    Removes the interval (start, end) from the given
    Interval_iterator. This is the lowest-level function that removes
    groups; this is where Interval objects are removed, truncated, or
    split.

    It is not required that the groups in the interval exist in this
    Gtid_set.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the next interval
    after end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @param lock If this function has to add or remove an interval,
    then this lock will be taken unless it is already taken.  This
    mechanism means that the lock will be taken lazily by
    e.g. add_gtid_set() the first time that the list of free intervals
    is accessed, and automatically released when add_gtid_set()
    returns.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove_gno_interval(Interval_iterator *ivitp,
                                         rpl_gno start, rpl_gno end,
                                         Free_intervals_lock *lock);
  /**
    Adds a list of intervals to the given SIDNO.

    The SIDNO must exist in the Gtid_set before this function is called.

    @param sidno The SIDNO to which intervals will be added.
    @param ivit Iterator over the intervals to add. This is typically
    an iterator over some other Gtid_set.
    @param lock If this function has to add or remove an interval,
    then this lock will be taken unless it is already taken.  This
    mechanism means that the lock will be taken lazily by
    e.g. add_gtid_set() the first time that the list of free intervals
    is accessed, and automatically released when add_gtid_set()
    returns.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_gno_intervals(rpl_sidno sidno,
                                       Const_interval_iterator ivit,
                                       Free_intervals_lock *lock);
  /**
    Removes a list of intervals from the given SIDNO.

    It is not required that the intervals exist in this Gtid_set.

    @param sidno The SIDNO from which intervals will be removed.
    @param ivit Iterator over the intervals to remove. This is typically
    an iterator over some other Gtid_set.
    @param lock If this function has to add or remove an interval,
    then this lock will be taken unless it is already taken.  This
    mechanism means that the lock will be taken lazily by
    e.g. add_gtid_set() the first time that the list of free intervals
    is accessed, and automatically released when add_gtid_set()
    returns.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status remove_gno_intervals(rpl_sidno sidno,
                                          Const_interval_iterator ivit,
                                          Free_intervals_lock *lock);

  /// Sid_map associated with this Gtid_set.
  Sid_map *sid_map;
  /**
    Array where the N'th element contains the head pointer to the
    intervals of SIDNO N+1.
  */
  DYNAMIC_ARRAY intervals;
  /// Linked list of free intervals.
  Interval *free_intervals;
  /// Linked list of chunks.
  Interval_chunk *chunks;
  /// The string length.
  mutable int cached_string_length;
  /// The String_format that was used when cached_string_length was computed.
  mutable const String_format *cached_string_format;
#ifndef DBUG_OFF
  /**
    The number of chunks.  Used only to check some invariants when
    DBUG is on.
  */
  int n_chunks;
#endif

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GTID_SET
  friend FRIEND_OF_GTID_SET;
#endif
  /// Only Free_intervals_lock is allowed to access free_intervals_mutex.
  friend class Gtid_set::Free_intervals_lock;
};


/**
  Holds information about a Gtid_set.  Can also be NULL.

  This is used as backend storage for @@session.gtid_next_list.  The
  idea is that we allow the user to set this to NULL, but we keep the
  Gtid_set object so that we can re-use the allocated memory and
  avoid costly allocations later.

  This is stored in struct system_variables (defined in sql_class.h),
  which is cleared using memset(0); hence the negated form of
  is_non_null.

  The convention is: if is_non_null is false, then the value of the
  session variable is NULL, and the field gtid_set may be NULL or
  non-NULL.  If is_non_null is true, then the value of the session
  variable is not NULL, and the field gtid_set has to be non-NULL.

  This is a POD. It has to be a POD because it is stored in
  THD::variables.
*/
struct Gtid_set_or_null
{
  /// Pointer to the Gtid_set.
  Gtid_set *gtid_set;
  /// True if this Gtid_set is NULL.
  bool is_non_null;
  /// Return NULL if this is NULL, otherwise return the Gtid_set.
  inline Gtid_set *get_gtid_set() const
  {
    DBUG_ASSERT(!(is_non_null && gtid_set == NULL));
    return is_non_null ? gtid_set : NULL;
  }
  /**
    Do nothing if this object is non-null; set to empty set otherwise.

    @return NULL if out of memory; Gtid_set otherwise.
  */
  Gtid_set *set_non_null(Sid_map *sm)
  {
    if (!is_non_null)
    {
      if (gtid_set == NULL)
        gtid_set= new Gtid_set(sm);
      else
        gtid_set->clear();
    }
    is_non_null= (gtid_set != NULL);
    return gtid_set;
  }
  /// Set this Gtid_set to NULL.
  inline void set_null() { is_non_null= false; }
};


/**
  Represents the set of GTIDs that are owned by some thread.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.

  The internal representation is a DYNAMIC_ARRAY that maps SIDNO to
  HASH, where each HASH maps GNO to my_thread_id.
*/
class Owned_gtids
{
public:
  /**
    Constructs a new, empty Owned_gtids object.

    @param sid_lock Read-write lock that protects updates to the
    number of SIDs.
  */
  Owned_gtids(Checkable_rwlock *sid_lock);
  /// Destroys this Owned_gtids.
  ~Owned_gtids();
  /**
    Add a GTID to this Owned_gtids.

    @param gtid The Gtid to add.
    @param owner The my_thread_id of the group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_gtid_owner(const Gtid &gtid, my_thread_id owner);
  /**
    Returns the owner of the given GTID, or 0 if the GTID is not owned.

    @param Gtid The Gtid to query.
    @return my_thread_id of the thread that owns the group, or
    0 if the group is not owned.
  */
  my_thread_id get_owner(const Gtid &gtid) const;
  /**
    Removes the given GTID.

    If the group does not exist in this Owned_gtids object, does
    nothing.

    @param gtid The Gtid.
  */
  void remove_gtid(const Gtid &gtid);
  /**
    Ensures that this Owned_gtids object can accomodate SIDNOs up to
    the given SIDNO.

    If this Owned_gtids object needs to be resized, then the lock
    will be temporarily upgraded to a write lock and then degraded to
    a read lock again; there will be a short period when the lock is
    not held at all.

    @param sidno The SIDNO.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno(rpl_sidno sidno);
  /// Returns true if there is a least one element of this Owned_gtids
  /// set in the other Gtid_set.
  bool is_intersection(const Gtid_set *other) const;
  /// Returns the maximal sidno that this Owned_gtids currently has space for.
  rpl_sidno get_max_sidno() const
  {
    sid_lock->assert_some_lock();
    return sidno_to_hash.elements;
  }

  /**
    Write a string representation of this Owned_groups to the given buffer.

    @param out Buffer to write to.
    @return Number of characters written.
  */
  int to_string(char *out) const
  {
    char *p= out;
    rpl_sidno max_sidno= get_max_sidno();
    rpl_sidno sid_map_max_sidno= global_sid_map->get_max_sidno();
    for (rpl_sidno sid_i= 0; sid_i < sid_map_max_sidno; sid_i++)
    {
      rpl_sidno sidno= global_sid_map->get_sorted_sidno(sid_i);
      if (sidno > max_sidno)
        continue;
      HASH *hash= get_hash(sidno);
      bool printed_sid= false;
      for (uint i= 0; i < hash->records; i++)
      {
        Node *node= (Node *)my_hash_element(hash, i);
        DBUG_ASSERT(node != NULL);
        if (!printed_sid)
        {
          p+= global_sid_map->sidno_to_sid(sidno).to_string(p);
          printed_sid= true;
        }
        p+= sprintf(p, ":%lld#%lu", node->gno, node->owner);
      }
    }
    *p= 0;
    return (int)(p - out);
  }

  /**
    Return an upper bound on the length of the string representation
    of this Owned_groups.  The actual length may be smaller.  This
    includes the trailing '\0'.
  */
  size_t get_max_string_length() const
  {
    rpl_sidno max_sidno= get_max_sidno();
    size_t ret= 0;
    for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    {
      HASH *hash= get_hash(sidno);
      if (hash->records > 0)
        ret+= rpl_sid::TEXT_LENGTH +
          hash->records * (1 + MAX_GNO_TEXT_LENGTH +
                           1 + MAX_THREAD_ID_TEXT_LENGTH);
    }
    return 1 + ret;
  }

  /**
    Return true if the given thread is the owner of any groups.
  */
  bool thread_owns_anything(my_thread_id thd_id) const
  {
    Gtid_iterator git(this);
    Node *node= git.get_node();
    while (node != NULL)
    {
      if (node->owner == thd_id)
        return true;
      git.next();
      node= git.get_node();
    }
    return false;
  }

#ifndef DBUG_OFF
  /**
    Debug only: return a newly allocated string representation of
    this Owned_gtids.
  */
  char *to_string() const
  {
    char *str= (char *)my_malloc(get_max_string_length(), MYF(MY_WME));
    DBUG_ASSERT(str != NULL);
    to_string(str);
    return str;
  }
  /// Debug only: print this Owned_gtids to stdout.
  void print() const
  {
    char *str= to_string();
    printf("%s\n", str);
    my_free(str);
  }
#endif
  /**
    Print this Owned_gtids to the trace file if debug is enabled; no-op
    otherwise.
  */
  void dbug_print(const char *text= "") const
  {
#ifndef DBUG_OFF
    char *str= to_string();
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", str));
    my_free(str);
#endif
  }
private:
  /// Represents one owned group.
  struct Node
  {
    /// GNO of the group.
    rpl_gno gno;
    /// Owner of the group.
    my_thread_id owner;
  };
  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Returns the HASH for the given SIDNO.
  HASH *get_hash(rpl_sidno sidno) const
  {
    DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
    sid_lock->assert_some_lock();
    return *dynamic_element(&sidno_to_hash, sidno - 1, HASH **);
  }
  /**
    Returns the Node for the given HASH and GNO, or NULL if the GNO
    does not exist in the HASH.
  */
  Node *get_node(const HASH *hash, rpl_gno gno) const
  {
    sid_lock->assert_some_lock();
    return (Node *)my_hash_search(hash, (const uchar *)&gno, sizeof(rpl_gno));
  }
  /**
    Returns the Node for the given group, or NULL if the group does
    not exist in this Owned_gtids object.
  */
  Node *get_node(const Gtid &gtid) const
  { return get_node(get_hash(gtid.sidno), gtid.gno); };
  /// Return true iff this Owned_gtids object contains the given group.
  bool contains_gtid(const Gtid &gtid) const { return get_node(gtid) != NULL; }
  /// Growable array of hashes.
  DYNAMIC_ARRAY sidno_to_hash;

public:
  /**
    Iterator over all groups in a Owned_gtids set.  This is a const
    iterator; it does not allow modification of the set.
  */
  class Gtid_iterator
  {
  public:
    Gtid_iterator(const Owned_gtids* og)
      : owned_gtids(og), sidno(1), hash(NULL), node_index(0), node(NULL)
    {
      max_sidno= owned_gtids->get_max_sidno();
      if (sidno <= max_sidno)
        hash= owned_gtids->get_hash(sidno);
      next();
    }
    /// Advance to next group.
    inline void next()
    {
#ifndef DBUG_OFF
      if (owned_gtids->sid_lock)
        owned_gtids->sid_lock->assert_some_wrlock();
#endif

      while (sidno <= max_sidno)
      {
        DBUG_ASSERT(hash != NULL);
        if (node_index < hash->records)
        {
          node= (Node *)my_hash_element(hash, node_index);
          DBUG_ASSERT(node != NULL);
          // Jump to next node on next iteration.
          node_index++;
          return;
        }

        node_index= 0;
        // hash is initialized on constructor or in previous iteration
        // for current SIDNO, so we must increment for next iteration.
        sidno++;
        if (sidno <= max_sidno)
          hash= owned_gtids->get_hash(sidno);
      }
      node= NULL;
    }
    /// Return next group, or {0,0} if we reached the end.
    inline Gtid get() const
    {
      Gtid ret= { 0, 0 };
      if (node)
      {
        ret.sidno= sidno;
        ret.gno= node->gno;
      }
      return ret;
    }
    /// Return next group Node, or NULL if we reached the end.
    inline Node* get_node() const
    {
      return node;
    }
  private:
    /// The Owned_gtids set we iterate over.
    const Owned_gtids *owned_gtids;
    /// The SIDNO of the current element, or 1 in the initial iteration.
    rpl_sidno sidno;
    /// Max SIDNO of the current iterator.
    rpl_sidno max_sidno;
    /// Current SIDNO hash.
    HASH *hash;
    /// Current node index on current SIDNO hash.
    uint node_index;
    /// Current node on current SIDNO hash.
    Node *node;
  };
};


/**
  Represents the state of the group log: the set of logged groups, the
  set of lost groups, the set of owned groups, the owner of each owned
  group, and a Mutex_cond_array that protects updates to groups of
  each SIDNO.

  Locking:

  This data structure has a read-write lock that protects the number
  of SIDNOs, and a Mutex_cond_array that contains one mutex per SIDNO.
  The rwlock is always the global_sid_lock.

  Access methods generally assert that the caller already holds the
  appropriate lock:

   - before accessing any global data, hold at least the rdlock.

   - before accessing a specific SIDNO in a Gtid_set or Owned_gtids
     (e.g., calling Gtid_set::_add_gtid(Gtid)), hold either the rdlock
     and the SIDNO's mutex lock; or the wrlock.  If you need to hold
     multiple mutexes, they must be acquired in order of increasing
     SIDNO.

   - before starting an operation that needs to access all SIDs
     (e.g. Gtid_set::to_string()), hold the wrlock.

  The access type (read/write) does not matter; the write lock only
  implies that the entire data structure is locked whereas the read
  lock implies that everything except SID-specific data is locked.
*/
class Gtid_state
{
public:
  /**
    Constructs a new Gtid_state object.

    @param _sid_lock Read-write lock that protects updates to the
    number of SIDs.
    @param _sid_map Sid_map used by this group log.
  */
  Gtid_state(Checkable_rwlock *_sid_lock, Sid_map *_sid_map)
    : sid_lock(_sid_lock),
    sid_map(_sid_map),
    sid_locks(sid_lock),
    logged_gtids(sid_map, sid_lock),
    lost_gtids(sid_map, sid_lock),
    owned_gtids(sid_lock) {}
  /**
    Add @@GLOBAL.SERVER_UUID to this binlog's Sid_map.

    This can't be done in the constructor because the constructor is
    invoked at server startup before SERVER_UUID is initialized.

    The caller must hold the read lock or write lock on sid_locks
    before invoking this function.

    @retval 0 Success
    @retval 1 Error (out of memory or IO error).
  */
  int init();
  /**
    Reset the state after RESET MASTER: remove all logged and lost
    groups, but keep owned groups as they are.

    The caller must hold the write lock on sid_lock before calling
    this function.
  */
  void clear();
  /**
    Returns true if the given GTID is logged.

    @param gtid The Gtid to check.

    @retval true The group is logged in the binary log.
    @retval false The group is not logged in the binary log.
  */
  bool is_logged(const Gtid &gtid) const
  {
    DBUG_ENTER("Gtid_state::is_logged");
    bool ret= logged_gtids.contains_gtid(gtid);
    DBUG_RETURN(ret);
  }
  /**
    Returns the owner of the given GTID, or 0 if the group is not owned.

    @param gtid The Gtid to check.
    @return my_thread_id of the thread that owns the group, or
    0 if the group is not owned.
  */
  my_thread_id get_owner(const Gtid &gtid) const
  { return owned_gtids.get_owner(gtid); }
#ifndef MYSQL_CLIENT
  /**
    Acquires ownership of the given GTID, on behalf of the given thread.

    The caller must lock the SIDNO before invoking this function.

    @param thd The thread that will own the GTID.
    @param gtid The Gtid to acquire ownership of.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status acquire_ownership(THD *thd, const Gtid &gtid);
  /**
    Update the state after the given thread has flushed cache to binlog.

    This will:
     - release ownership of all GTIDs owned by the THD;
     - add all GTIDs in the Group_cache to
       logged_gtids;
     - send a broadcast on the condition variable for every sidno for
       which we released ownership.

    @param thd Thread for which owned groups are updated.
  */
  enum_return_status update_on_flush(THD *thd);
  /**
    Remove the GTID owned by thread from owned GTIDs, stating that
    thd->owned_gtid was committed.

    This will:
     - remove owned GTID from owned_gtids;
     - remove all owned GTIDS from thd->owned_gtid and thd->owned_gtid_set;

    @param thd Thread for which owned groups are updated.
  */
  void update_on_commit(THD *thd);
  /**
    Update the state after the given thread has rollbacked.

    This will:
     - release ownership of all GTIDs owned by the THD;
     - remove owned GTID from owned_gtids;
     - remove all owned GTIDS from thd->owned_gtid and thd->owned_gtid_set;
     - send a broadcast on the condition variable for every sidno for
       which we released ownership.

    @param thd Thread for which owned groups are updated.
  */
  void update_on_rollback(THD *thd);
#endif // ifndef MYSQL_CLIENT
  /**
    Allocates a GNO for an automatically numbered group.

    @param sidno The group's SIDNO.

    @retval negative the numeric value of GS_ERROR_OUT_OF_MEMORY
    @retval other The GNO for the group.
  */
  rpl_gno get_automatic_gno(rpl_sidno sidno) const;
  /// Locks a mutex for the given SIDNO.
  void lock_sidno(rpl_sidno sidno) { sid_locks.lock(sidno); }
  /// Unlocks a mutex for the given SIDNO.
  void unlock_sidno(rpl_sidno sidno) { sid_locks.unlock(sidno); }
  /// Broadcasts updates for the given SIDNO.
  void broadcast_sidno(rpl_sidno sidno) { sid_locks.broadcast(sidno); }
  /// Assert that we own the given SIDNO.
  void assert_sidno_lock_owner(rpl_sidno sidno)
  { sid_locks.assert_owner(sidno); }
#ifndef MYSQL_CLIENT
  /**
    Waits until the given GTID is not owned by any other thread.

    This requires that the caller holds a read lock on sid_lock.  It
    will release the lock before waiting; neither global_sid_lock nor
    the mutex lock on SIDNO will not be held when this function
    returns.

    @param thd THD object of the caller.
    @param g Gtid to wait for.
  */
  void wait_for_gtid(THD *thd, const Gtid &gtid);
#endif // ifndef MYSQL_CLIENT
#ifdef HAVE_NDB_BINLOG
  /**
    Locks one mutex for each SIDNO where the given Gtid_set has at
    least one GTID.  Locks are acquired in order of increasing SIDNO.
  */
  void lock_sidnos(const Gtid_set *set);
  /**
    Unlocks the mutex for each SIDNO where the given Gtid_set has at
    least one GTID.
  */
  void unlock_sidnos(const Gtid_set *set);
  /**
    Broadcasts the condition variable for each SIDNO where the given
    Gtid_set has at least one GTID.
  */
  void broadcast_sidnos(const Gtid_set *set);
#endif // ifdef HAVE_NDB_BINLOG
  /**
    Ensure that owned_gtids, logged_gtids, lost_gtids, and sid_locks
    have room for at least as many SIDNOs as sid_map.

    This function must only be called in one place:
    Sid_map::add_sid().

    Requires that the write lock on sid_locks is held.  If any object
    needs to be resized, then the lock will be temporarily upgraded to
    a write lock and then degraded to a read lock again; there will be
    a short period when the lock is not held at all.

    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status ensure_sidno();
  /// Return a pointer to the Gtid_set that contains the logged groups.
  const Gtid_set *get_logged_gtids() const { return &logged_gtids; }
  /// Return a pointer to the Gtid_set that contains the logged groups.
  const Gtid_set *get_lost_gtids() const { return &lost_gtids; }
  /// Return a pointer to the Owned_gtids that contains the owned groups.
  const Owned_gtids *get_owned_gtids() const { return &owned_gtids; }
  /// Return the server's SID's SIDNO
  rpl_sidno get_server_sidno() const { return server_sidno; }
#ifndef DBUG_OFF
  /**
    Debug only: Returns an upper bound on the length of the string
    generated by to_string(), not counting '\0'.  The actual length
    may be shorter.
  */
  size_t get_max_string_length() const
  {
    return owned_gtids.get_max_string_length() +
      logged_gtids.get_string_length() +
      lost_gtids.get_string_length() +
      100;
  }
  /// Debug only: Generate a string in the given buffer and return the length.
  int to_string(char *buf) const
  {
    char *p= buf;
    p+= sprintf(p, "Logged GTIDs:\n");
    p+= logged_gtids.to_string(p);
    p+= sprintf(p, "\nOwned GTIDs:\n");
    p+= owned_gtids.to_string(p);
    p+= sprintf(p, "\nLost GTIDs:\n");
    p+= lost_gtids.to_string(p);
    return (int)(p - buf);
  }
  /// Debug only: return a newly allocated string, or NULL on out-of-memory.
  char *to_string() const
  {
    char *str= (char *)my_malloc(get_max_string_length(), MYF(MY_WME));
    to_string(str);
    return str;
  }
  /// Debug only: print this Gtid_state to stdout.
  void print() const
  {
    char *str= to_string();
    printf("%s", str);
    my_free(str);
  }
#endif
  /**
    Print this Gtid_state to the trace file if debug is enabled; no-op
    otherwise.
  */
  void dbug_print(const char *text= "") const
  {
#ifndef DBUG_OFF
    sid_lock->assert_some_wrlock();
    char *str= to_string();
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", str));
    my_free(str);
#endif
  }
private:
#ifdef HAVE_NDB_BINLOG
  /// Lock all SIDNOs owned by the given THD.
  void lock_owned_sidnos(const THD *thd);
#endif
  /// Unlock all SIDNOs owned by the given THD.
  void unlock_owned_sidnos(const THD *thd);
  /// Broadcast the condition for all SIDNOs owned by the given THD.
  void broadcast_owned_sidnos(const THD *thd);
  /**
    Remove the GTID owned by thread from owned GTIDs.

    @param thd Thread for which owned groups are updated.
    @param is_commit send a broadcast on the condition variable for
           every sidno for which we released ownership.
  */
  void update_owned_gtids_impl(THD *thd, bool is_commit);


  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// The Sid_map used by this Gtid_state.
  mutable Sid_map *sid_map;
  /// Contains one mutex/cond pair for every SIDNO.
  Mutex_cond_array sid_locks;
  /// The set of GTIDs that have been executed and logged (and possibly purged).
  Gtid_set logged_gtids;
  /**
    The set of GTIDs that existed in some previously purged binary log.
    This is always a subset of logged_gtids.
  */
  Gtid_set lost_gtids;
  /// The set of GTIDs that are owned by some thread.
  Owned_gtids owned_gtids;
  /// The SIDNO for this server.
  rpl_sidno server_sidno;

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GTID_STATE
  friend FRIEND_OF_GTID_STATE;
#endif
};


/// Global state of GTIDs.
extern Gtid_state *gtid_state;


/**
  Enumeration of group types.
*/
enum enum_group_type
{
  /**
    It is important that AUTOMATIC_GROUP==0 so that the default value
    for thd->variables->gtid_next.type is AUTOMATIC_GROUP.
  */
  AUTOMATIC_GROUP= 0, GTID_GROUP, ANONYMOUS_GROUP, INVALID_GROUP
};


/**
  This struct represents a specification of a GTID for a statement to
  be executed: either "AUTOMATIC", "ANONYMOUS", or "SID:GNO".

  This is a POD. It has to be a POD because it is used in THD::variables.
*/
struct Gtid_specification
{
  /// The type of this GTID
  enum_group_type type;
  /**
    The GTID:
    { SIDNO, GNO } if type == GTID;
    { 0, 0 } if type == AUTOMATIC or ANONYMOUS.
  */
  Gtid gtid;
  /// Set the type to GTID_GROUP and SID, GNO to the given values.
  void set(rpl_sidno sidno, rpl_gno gno)
  { type= GTID_GROUP; gtid.sidno= sidno; gtid.gno= gno; }
  /// Set the type to GTID_GROUP and SID, GNO to the given Gtid.
  void set(const Gtid &gtid_param) { set(gtid_param.sidno, gtid_param.gno); }
  /// Set the type to GTID_GROUP and SID, GNO to 0, 0.
  void clear() { set(0, 0); }
  /// Return true if this Gtid_specification is equal to 'other'.
  bool equals(const Gtid_specification &other) const
  {
    return (type == other.type &&
            (type != GTID_GROUP || gtid.equals(other.gtid)));
  }
  /**
    Return true if this Gtid_specification is a GTID_GROUP with the
    same SID, GNO as 'other_gtid'.
  */
  bool equals(const Gtid &other_gtid) const
  { return type == GTID_GROUP && gtid.equals(other_gtid); }
#ifndef MYSQL_CLIENT
  /**
    Parses the given string and stores in this Gtid_specification.

    @param text The text to parse
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status parse(Sid_map *sid_map, const char *text);
  /**
    Returns the type of the group, if the given string is a valid Gtid_specification; INVALID otherwise.
  */
  static enum_group_type get_type(const char *text);
  /// Returns true if the given string is a valid Gtid_specification.
  static bool is_valid(const char *text)
  { return Gtid_specification::get_type(text) != INVALID_GROUP; }
#endif
  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  /**
    Writes this Gtid_specification to the given string buffer.

    @param sid_map Sid_map to use if the type of this
    Gtid_specification is GTID_GROUP.
    @param buf[out] The buffer
    @retval The number of characters written.
  */
  int to_string(const Sid_map *sid_map, char *buf) const;
  /**
    Writes this Gtid_specification to the given string buffer.

    @param sid SID to use if the type of this Gtid_specification is
    GTID_GROUP.  Can be NULL if this Gtid_specification is
    ANONYMOUS_GROUP or AUTOMATIC_GROUP.
    @param buf[out] The buffer
    @retval The number of characters written.
    @buf[out]
  */
  int to_string(const rpl_sid *sid, char *buf) const;
#ifndef DBUG_OFF
  /// Debug only: print this Gtid_specification to stdout.
  void print() const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(global_sid_map, buf);
    printf("%s\n", buf);
  }
#endif
  /**
    Print this Gtid_specificatoin to the trace file if debug is
    enabled; no-op otherwise.
  */
  void dbug_print(const char *text= "") const
  {
#ifndef DBUG_OFF
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(global_sid_map, buf);
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", buf));
#endif
  }
};


/**
  Represents a group in the group cache.

  Groups in the group cache are slightly different from other groups,
  because not all information about them is known.

  Automatic groups are marked as such by setting gno<=0.
*/
struct Cached_group
{
  /// The gtid for this group.
  Gtid_specification spec;
  /**
    The position of this GTID in the cache, i.e., the total size of
    all previous groups.
  */
  rpl_binlog_pos binlog_offset;
};


/**
  Represents a group cache: either the statement group cache or the
  transaction group cache.
*/
class Group_cache
{
public:
  /// Constructs a new Group_cache.
  Group_cache();
  /// Deletes a Group_cache.
  ~Group_cache();
  /// Removes all groups from this cache.
  void clear();
  /// Return the number of groups in this group cache.
  inline int get_n_groups() const { return groups.elements; }
  /// Return true iff the group cache contains zero groups.
  inline bool is_empty() const { return get_n_groups() == 0; }
  /**
    Adds a group to this Group_cache.  The group should
    already have been written to the stmt or trx cache.  The SIDNO and
    GNO fields are taken from @@SESSION.GTID_NEXT.

    @param thd The THD object from which we read session variables.
    @param binlog_length Length of group in binary log.
    @retval EXTEND_EXISTING_GROUP The last existing group had the same GTID
    and has been extended to include this group too.
    @retval APPEND_NEW_GROUP The group has been appended to this cache.
    @retval ERROR An error (out of memory) occurred.
    The error has been reported.
  */
  enum enum_add_group_status
  {
    EXTEND_EXISTING_GROUP, APPEND_NEW_GROUP, ERROR
  };
#ifndef MYSQL_CLIENT
  enum_add_group_status
    add_logged_group(const THD *thd, my_off_t binlog_offset);
#endif // ifndef MYSQL_CLIENT
#ifdef NON_DISABLED_GTID
  /**
    Adds an empty group with the given (SIDNO, GNO) to this cache.

    @param gtid The GTID of the group.

    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_add_group_status add_empty_group(const Gtid &gtid);
#endif // ifdef NON_DISABLED_GTID
#ifndef MYSQL_CLIENT
  /**
    Write all gtids in this cache to the global Gtid_state.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status write_to_gtid_state() const;
  /**
    Generates GNO for all groups that are committed for the first time
    in this Group_cache.

    This acquires ownership of all groups.  After this call, this
    Group_cache does not contain any Cached_groups that have
    type==GTID_GROUP and gno<=0.

    @param thd The THD that this Gtid_state belongs to.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR
  */
  enum_return_status generate_automatic_gno(THD *thd);
#endif // ifndef MYSQL_CLIENT
  /**
    Return true if this Group_cache contains the given GTID.

    @param gtid The Gtid to check.
    @retval true The group exists in this cache.
    @retval false The group does not exist in this cache.
  */
  bool contains_gtid(const Gtid &gtid) const;
  /**
    Add all GTIDs that exist in this Group_cache to the given Gtid_set.

    @param gs The Gtid_set to which groups are added.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status get_gtids(Gtid_set *gs) const;

#ifndef DBUG_OFF
  /**
    Debug only: store a textual representation of this Group_cache in
    the given buffer and return the length.
  */
  size_t to_string(const Sid_map *sm, char *buf) const
  {
    int n_groups= get_n_groups();
    char *s= buf;

    s += sprintf(s, "%d groups = {\n", n_groups);
    for (int i= 0; i < n_groups; i++)
    {
      Cached_group *group= get_unsafe_pointer(i);
      char uuid[Uuid::TEXT_LENGTH + 1]= "[]";
      if (group->spec.gtid.sidno)
        sm->sidno_to_sid(group->spec.gtid.sidno).to_string(uuid);
      s += sprintf(s, "  %s:%lld [offset %lld] %s\n",
                   uuid, group->spec.gtid.gno, group->binlog_offset,
                   group->spec.type == GTID_GROUP ? "GTID" :
                   group->spec.type == ANONYMOUS_GROUP ? "ANONYMOUS" :
                   group->spec.type == AUTOMATIC_GROUP ? "AUTOMATIC" :
                   "INVALID-GROUP-TYPE");
    }
    sprintf(s, "}\n");
    return s - buf;
  }
  /**
    Debug only: return an upper bound on the length of the string
    generated by to_string(). The actual length may be shorter.
  */
  size_t get_max_string_length() const
  {
    return (2 + Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH + 4 + 2 +
            40 + 10 + 21 + 1 + 100/*margin*/) * get_n_groups() + 100/*margin*/;
  }
  /**
    Debug only: generate a textual representation of this Group_cache
    and store in a newly allocated string. Return the string, or NULL
    on out of memory.
  */
  char *to_string(const Sid_map *sm) const
  {
    char *str= (char *)my_malloc(get_max_string_length(), MYF(MY_WME));
    if (str)
      to_string(sm, str);
    return str;
  }
  /// Debug only: print this Group_cache to stdout.
  void print(const Sid_map *sm) const
  {
    char *str= to_string(sm);
    printf("%s\n", str);
    my_free(str);
  }
#endif
  /**
    Print this Gtid_cache to the trace file if debug is enabled; no-op
    otherwise.
  */
  void dbug_print(const Sid_map *sid_map, const char *text= "") const
  {
#ifndef DBUG_OFF
    char *str= to_string(sid_map);
    DBUG_PRINT("info", ("%s%s%s", text, *text ? ": " : "", str));
    my_free(str);
#endif
  }

  /**
    Returns a pointer to the given group.  The pointer is only valid
    until the next time a group is added or removed.

    @param index Index of the element: 0 <= index < get_n_groups().
  */
  inline Cached_group *get_unsafe_pointer(int index) const
  {
    DBUG_ASSERT(index >= 0 && index < get_n_groups());
    return dynamic_element(&groups, index, Cached_group *);
  }

private:
  /// List of all groups in this cache, of type Cached_group.
  DYNAMIC_ARRAY groups;

  /**
    Return a pointer to the last group, or NULL if this Group_cache is
    empty.
  */
  Cached_group *get_last_group()
  {
    int n_groups= get_n_groups();
    return n_groups == 0 ? NULL : get_unsafe_pointer(n_groups - 1);
  }

  /**
    Allocate space for one more group and return a pointer to it, or
    NULL on error.
  */
  Cached_group *allocate_group()
  {
    Cached_group *ret= (Cached_group *)alloc_dynamic(&groups);
    if (ret == NULL)
      BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
    return ret;
  }

  /**
    Adds the given group to this group cache, or merges it with the
    last existing group in the cache if they are compatible.

    @param group The group to add.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status add_group(const Cached_group *group);
  /**
    Prepare the cache to be written to the group log.

    @todo The group log is not yet implemented. /Sven

    @param trx_group_cache @see write_to_log.
    @return RETURN_STATUS_OK or RETURN_STATUS_REPORTED_ERROR.
  */
  enum_return_status
    write_to_log_prepare(Group_cache *trx_group_cache,
                         rpl_binlog_pos offset_after_last_statement,
                         Cached_group **last_non_empty_group);

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_CACHE
  friend FRIEND_OF_GROUP_CACHE;
#endif
};

/**
  Indicates if a statement should be skipped or not. Used as return
  value from gtid_before_statement.
*/
enum enum_gtid_statement_status
{
  /// Statement can execute.
  GTID_STATEMENT_EXECUTE,
  /// Statement should be cancelled.
  GTID_STATEMENT_CANCEL,
  /**
    Statement should be skipped, but there may be an implicit commit
    after the statement if gtid_commit is set.
  */
  GTID_STATEMENT_SKIP
};


#ifndef MYSQL_CLIENT
/**
  Before a loggable statement begins, this function:

   - checks that the various @@session.gtid_* variables are consistent
     with each other

   - starts the super-group (if no super-group is active) and acquires
     ownership of all groups in the super-group

   - starts the group (if no group is active)
*/
enum_gtid_statement_status
gtid_before_statement(THD *thd, Group_cache *gsc, Group_cache *gtc);

/**
  Check that the current statement does not contradict
  disable_gtid_unsafe_statements, that there is no implicit commit in
  a transaction when GTID_NEXT!=AUTOMATIC, and whether the statement
  should be cancelled.

  @param thd THD object for the session.
*/
enum_gtid_statement_status gtid_pre_statement_checks(const THD *thd);

/**
  When a transaction is rolled back, this function releases ownership
  of any GTIDs that the transaction owns.
*/
int gtid_rollback(THD *thd);

int gtid_acquire_ownership_single(THD *thd);
#ifdef HAVE_NDB_BINLOG
int gtid_acquire_ownership_multiple(THD *thd);
#endif

#endif // ifndef MYSQL_CLIENT

#endif /* RPL_GTID_H_INCLUDED */
