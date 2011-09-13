/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#ifndef RPL_GROUPS_H_INCLUDED
#define RPL_GROUPS_H_INCLUDED


#include "my_base.h"


/*
  In the current version, enable UGID only in debug builds.  We will
  enable it fully when it is more complete.
*/
#ifndef NO_DBUG
/*
  The group log can only be correctly truncated if my_chsize actually
  truncates the file. So disable UGIDs on platforms that don't support
  truncate.
*/
#if defined(_WIN32) || defined(HAVE_FTRUNCATE) || defined(HAVE_CHSIZE)
#define HAVE_UGID
#endif
#endif


#ifdef HAVE_UGID

#include "mysqld.h"
#include "hash.h"
#include "lf.h"
#include "my_atomic.h"


class MYSQL_BIN_LOG;


typedef int64 rpl_gno;
typedef int32 rpl_sidno;
typedef int64 rpl_binlog_no;
typedef int64 rpl_binlog_pos;
typedef int64 rpl_lgid;

/**
  General 'return value type' for functions that can fail.

  The numerical values should be zero or negative: this allows us to
  store them in rpl_sidno and rpl_gno while reserving positive values
  for correct SIDs and GNOs.
*/
enum enum_group_status
{
  GS_SUCCESS= 0,
  GS_ERROR_OUT_OF_MEMORY= -1,
  GS_ERROR_PARSE= -2,
  GS_ERROR_IO= -3
};

/**
  Given a value of type enum_group_status: if the value is GS_SUCCESS, do nothing; otherwise return the value. This is used to propagate errors to the caller.
*/
#define GROUP_STATUS_THROW(VAL)                                         \
  do                                                                    \
  {                                                                     \
    enum_group_status _group_status_throw_val= VAL;                     \
    if ( _group_status_throw_val != GS_SUCCESS)                         \
      DBUG_RETURN(_group_status_throw_val);                             \
  } while (0)


const rpl_gno MAX_GNO= LONGLONG_MAX;
const rpl_sidno ANONYMOUS_SIDNO= 0;
const int MAX_GNO_TEXT_LENGTH= 19;


/**
  Parse a GNO from a string.

  @param s Pointer to the string. *s will advance to the end of the
  parsed GNO, if a correct GNO is found.
  @retval GNO if a correct GNO was found.
  @retval 0 otherwise.
*/
rpl_gno parse_gno(const char **s);
/**
  Formats a GNO as a string.

  @param s The buffer.
  @param gno The GNO.
  @return Length of the generated string.
*/
int format_gno(char *s, rpl_gno gno);


struct Rpl_owner_id
{
  int owner_type;
  uint32 thread_id;
  void copy_from(const THD *thd);
  void set_to_dead_client() { owner_type= thread_id= 0; }
  void set_to_none() { owner_type= thread_id= -1; }
  bool equals(const THD *thd) const;
  bool is_sql_thread() const { return owner_type >= 1; }
  bool is_none() const { return owner_type == -1; }
  bool is_client() const { return owner_type == 0; }
  bool is_very_old_client() const { return owner_type == 0 && thread_id == 0; }
  bool is_live_client() const;
  bool is_dead_client() const
  { return is_client() && !is_very_old_client() && !is_live_client(); }
};


/**
  Represents a UUID.

  This is a POD.
*/
struct Uuid
{
  /**
    Stores the UUID represented by a string on the form
    XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX in this object.
  */
  enum_group_status parse(const char *string);
  /**
    Copies the given 16-byte data to this UUID.
  */
  void copy_from(const unsigned char *data)
  { memcpy(bytes, data, BYTE_LENGTH); }
  /**
    Copies the given UUID object to this UUID.
  */
  void copy_from(const Uuid *data) { copy_from(data->bytes); }
  /**
    Returns true if this UUID is equal the given UUID.
  */
  bool equals(const Uuid *other) const
  {
    return memcmp(bytes, other->bytes, BYTE_LENGTH) == 0;
  }
  /**
    Generates a 36+1 character long representation of this UUID object
    in the given string buffer.

    @retval 36 - the length of the resulting string.
  */
  int to_string(char *buf) const;
#ifndef NO_DBUG
  void print() const
  {
    char buf[TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
#endif
  /**
    Returns true if the given string contains a valid UUID, false otherwise.
  */
  static bool is_valid(const char *string);
  unsigned char bytes[16];
  static const int TEXT_LENGTH= 36;
  static const int BYTE_LENGTH= 16;
  static const int BIT_LENGTH= 128;
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
#ifndef NO_DBUG
    my_atomic_rwlock_init(&atomic_lock);
    lock_state= 0;
#endif
    mysql_rwlock_init(0, &rwlock);
  }
  /// Destroy this Checkable_lock.
  ~Checkable_rwlock()
  {
#ifndef NO_DBUG
    my_atomic_rwlock_destroy(&atomic_lock);
#endif
    mysql_rwlock_destroy(&rwlock);
  }

  /// Acquire the read lock.
  inline void rdlock()
  {
    mysql_rwlock_rdlock(&rwlock);
    assert_no_wrlock();
#ifndef NO_DBUG
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
#ifndef NO_DBUG
    my_atomic_rwlock_wrlock(&atomic_lock);
    my_atomic_store32(&lock_state, -1);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
  }
  /// Release the lock (whether it is a write or read lock).
  inline void unlock()
  {
    assert_some_lock();
#ifndef NO_DBUG
    my_atomic_rwlock_wrlock(&atomic_lock);
    int val= my_atomic_load32(&lock_state);
    if (val > 0)
      my_atomic_add32(&lock_state, -1);
    else if (val == -1)
      my_atomic_store32(&lock_state, 0);
    else
      DBUG_ASSERT(0);
    my_atomic_rwlock_wrunlock(&atomic_lock);
#endif
    mysql_rwlock_unlock(&rwlock);
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
#ifndef NO_DBUG
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
#endif
  /// The rwlock.
  mysql_rwlock_t rwlock;
};


/**
  Represents a bidirectional map between SID and SIDNO.

  SIDNOs are always numbers greater or equal to 1.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
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
  /**
    Permanently add the given SID to this map if it does not already
    exist.

    The caller must hold the read lock on sid_lock before invoking
    this function.  If the SID does not exist in this map, it will
    release the read lock, take a write lock, update the map, release
    the write lock, and take the read lock again.

    @param sid The SID.
    @param flush Flush changes to disk.
    @retval SIDNO The SIDNO for the SID (a new SIDNO if the SID did
    not exist, an existing if it did exist).
    @retval GS_ERROR_IO or GS_ERROR_OUT_OF_MEMORY if there is an error.

    @note The SID is stored on disk forever.  This is needed if the
    SID is written to the binary log.  If the SID will not be written
    to the binary log, it is a waste of disk space.  If this becomes a
    problem, we may add add_temporary(), which would only store the
    sid in memory, and return a negative sidno.
  */
  rpl_sidno add_permanent(const rpl_sid *sid, bool flush= 1);
  /**
    Write changes to disk.

    This is only meaningful if add_permanent(sid, 0) has been
    called.

    @retval GS_SUCCESS success.
    @retval GS_ERROR_IO error.
  */
  enum_group_status flush();
  /**
    Get the SIDNO for a given SID

    The caller must hold the read lock on sid_lock before invoking
    this function.

    @param sid The SID.
    @retval SIDNO if the given SID exists in this map.
    @retval 0 if the given SID does not exist in this map.
  */
  rpl_sidno sid_to_sidno(const rpl_sid *sid) const
  {
    sid_lock->assert_some_lock();
    Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
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
  const rpl_sid *sidno_to_sid(rpl_sidno sidno) const
  {
    sid_lock->assert_some_lock();
    DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
    return &(*dynamic_element(&_sidno_to_sid, sidno - 1, Node **))->sid;
  }
  /**
    Return the n'th smallest sidno, in the order of the SID's UUID.

    The caller must hold the read lock on sid_lock before invoking
    this function.

    @param n A number in the interval [0, get_max_sidno()-1], inclusively.
  */
  rpl_sidno get_sorted_sidno(rpl_sidno n) const
  {
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


/**
  Represents a growable array where each element contains a mutex and
  a condition variable.

  Each element can be locked, unlocked, broadcast, or waited for, and
  it is possible to call "THD::enter_cond" for the condition.

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
    This is a no-op if NO_DBUG is on.
  */
  inline void assert_owner(int n) const
  {
#ifndef NO_DBUG
    mysql_mutex_assert_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /**
    Assert that this thread does not own the n'th mutex.
    This is a no-op if NO_DBUG is on.
  */
  inline void assert_not_owner(int n) const
  {
#ifndef NO_DBUG
    mysql_mutex_assert_not_owner(&get_mutex_cond(n)->mutex);
#endif
  }
  /// Wait for signal on the n'th condition variable.
  inline void wait(int n) const
  {
    DBUG_ENTER("Mutex_cond_array::wait");
    Mutex_cond *mutex_cond= get_mutex_cond(n);
    mysql_mutex_assert_owner(&mutex_cond->mutex);
    mysql_cond_wait(&mutex_cond->cond, &mutex_cond->mutex);
    DBUG_VOID_RETURN;
  }
  /// Execute THD::enter_cond for the n'th condition variable.
  void enter_cond(THD *thd, int n, PSI_stage_info *stage,
                  PSI_stage_info *old_stage) const;
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
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status ensure_index(int n);
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
  Holds information about a group: the sidno and the gno.
*/
struct Group
{
  rpl_sidno sidno;
  rpl_gno gno;

  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  static bool is_valid(const char *text);
  int to_string(const Sid_map *sid_map, char *buf) const;
  /**
    Parses the given string and stores in this Group.

    @param text The text to parse
    @return GS_SUCCESS or GS_ERROR_PARSE
  */
  enum_group_status parse(Sid_map *sid_map, const char *text);

#ifndef NO_DBUG
  void print(const Sid_map *sid_map) const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(sid_map, buf);
    printf("%s\n", buf);
  }
#endif
};


/**
  Represents a set of groups.

  This is structured as an array, indexed by SIDNO, where each element
  contains a linked list of intervals.

  This data structure OPTIONALLY has a read-write lock that protects
  the number of SIDNOs.  The lock is provided by the invoker of the
  constructor and it is generally the caller's responsibility to
  acquire the read lock.  If the lock is not NULL, access methods
  assert that the caller already holds the read (or write) lock.  If
  the lock is not NULL and a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Group_set
{
public:
  /**
    Constructs a new, empty Group_set.

    @param sid_map The Sid_map to use.
    @param sid_lock Read-write lock that protects updates to the
    number of SIDs. This may be NULL if such changes do not need to be
    protected.
  */
  Group_set(Sid_map *sid_map, Checkable_rwlock *sid_lock= NULL);
  /**
    Constructs a new Group_set that contains the groups in the given string, in the same format as add(char *).

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
  Group_set(Sid_map *sid_map, const char *text, enum_group_status *status,
            Checkable_rwlock *sid_lock= NULL);
  /**
    Constructs a new Group_set that shares the same sid_map and
    sid_lock objects and contains a copy of all groups.

    @param other The Group_set to copy.
    @param status Will be set to GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  Group_set(Group_set *other, enum_group_status *status);
  //Group_set(Sid_map *sid_map, Group_set *relative_to, Sid_map *sid_map_enc, const unsigned char *encoded, int length, enum_group_status *status);
  /// Destroy this Group_set.
  ~Group_set();
  //static int encode(Group_set *relative_to, Sid_map *sid_map_enc, unsigned char *buf);
  //static int encoded_length(Group_set *relative_to, Sid_map *sid_map_enc);
  /**
    Removes all groups from this Group_set.

    This does not deallocate anything: if groups are added later,
    existing allocated memory will be re-used.
  */
  void clear();
  /**
    Adds the given group to this Group_set.

    The SIDNO must exist in the Group_set before this function is called.

    @param sidno SIDNO of the group to add.
    @param gno GNO of the group to add.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status _add(rpl_sidno sidno, rpl_gno gno)
  {
    Interval_iterator ivit(this, sidno);
    return add(&ivit, gno, gno + 1);
  }
  /**
    Adds all groups from the given Group_set to this Group_set.

    If sid_lock != NULL, then the read lock must be held before
    calling this function. If a new sidno is added so that the array
    of lists of intervals is grown, sid_lock is temporarily upgraded
    to a write lock and then degraded again; there will be a short
    period when the lock is not held at all.

    @param other The group set to add.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status add(const Group_set *other);
  /**
    Removes all groups in the given Group_set from this Group_set.

    @param other The group set to remove.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status remove(const Group_set *other);
  /**
    Adds the set of groups represented by the given string to this Group_set.

    The string must have the format of a comma-separated list of zero
    or more of the following:

       XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX(:NUMBER+(-NUMBER)?)*

       Each X is a hexadecimal digit (upper- or lowercase).
       NUMBER is a decimal, 0xhex, or 0oct number.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If a new sidno is added so that the
    array of lists of intervals is grown, sid_lock is temporarily
    upgraded to a write lock and then degraded again; there will be a
    short period when the lock is not held at all.

    @param text The string to parse.
    @return GS_SUCCESS or GS_ERROR_PARSE or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status add(const char *text);
  /**
    Decodes a Group_set from the given string.

    @param string The string to parse.
    @param length The number of bytes.
    @return GS_SUCCESS or GS_ERROR_PARSE or GS_ERROR_OUT_OF_MEMORY
  */
  //int add(const unsigned char *encoded, int length);
  /// Return true iff the given group exists in this set.
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const;
  /// Returns the maximal sidno that this Group_set currently has space for.
  rpl_sidno get_max_sidno() const
  {
    if (sid_lock)
      sid_lock->assert_some_lock();
    return intervals.elements;
  }
  /**
    Allocates space for all sidnos up to the given sidno in the array of intervals.
    The sidno must exist in the Sid_map associated with this Group_set.

    If sid_lock != NULL, then the read lock on sid_lock must be held
    before calling this function. If the array is grown, sid_lock is
    temporarily upgraded to a write lock and then degraded again;
    there will be a short period when the lock is not held at all.

    @param sidno The SIDNO.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status ensure_sidno(rpl_sidno sidno);
  /// Returns true if this Group_set is equal to the other Group_set.
  bool equals(const Group_set *other) const;
  /// Returns true if this Group_set is a subset of the other Group_set.
  bool is_subset(const Group_set *super) const;
  /// Returns true if this Group_set is empty.
  bool is_empty() const
  {
    Group_iterator git(this);
    return git.get().sidno == 0;
  }
  /**
    Returns 0 if this Group_set is empty, 1 if it contains exactly one
    group, and 2 if it contains more than one group.

    This can be useful to check if the group is a singleton set or not.
  */
  int zero_one_or_many() const
  {
    Group_iterator git(this);
    if (git.get().sidno == 0)
      return 0;
    git.next();
    if (git.get().sidno == 0)
      return 1;
    return 2;
  }
  /**
    Returns true if this Group_set contains at least one group with
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
    Returns true if the given string is a valid specification of a Group_set, false otherwise.
  */
  static bool is_valid(const char *text);
#ifndef NO_DBUG
  /// Print this group set to stdout.
  void print() const
  {
    char *buf= (char *)malloc(get_string_length() + 1);
    DBUG_ASSERT(buf != NULL);
    to_string(buf);
    printf("%s\n", buf);
    free(buf);
  }
#endif
  //bool is_intersection_nonempty(Group_set *other);
  //Group_set in_place_intersection(Group_set other);
  //Group_set in_place_complement(Sid_map map);


  /**
    Class Group_set::String_format defines the separators used by
    Group_set::to_string.
  */
  struct String_format
  {
    const char *begin;
    const char *end;
    const char *sid_gno_separator;
    const char *gno_start_end_separator;
    const char *gno_gno_separator;
    const char *gno_sid_separator;
    const int begin_length;
    const int end_length;
    const int sid_gno_separator_length;
    const int gno_start_end_separator_length;
    const int gno_gno_separator_length;
    const int gno_sid_separator_length;
  };
  /**
    Returns the length of the output from to_string.

    @param string_format String_format object that specifies
    separators in the resulting text.
  */
  int get_string_length(const String_format *string_format=
                        &default_string_format) const;
  /**
    Encodes this Group_set as a string.

    @param buf Pointer to the buffer where the string should be
    stored. This should have size at least get_string_length()+1.
    @param string_format String_format object that specifies
    separators in the resulting text.
    @return Length of the generated string.
  */
  int to_string(char *buf, const String_format *string_format=
                &default_string_format) const;
  /// The default String_format: the format understood by add(const char *).
  static const String_format default_string_format;
  /**
    String_format useful to generate an SQL string: the string is
    wrapped in single quotes and there is a newline between SIDs.
  */
  static const String_format sql_string_format;

  /// Return the Sid_map associated with this Group_set.
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
    bool equals(const Interval *other) const
    {
      return start == other->start && end == other->end;
    }
    /// Pointer to next interval in list.
    Interval *next;
  };

  /**
    Provides an array of Intervals that this Group_set can use when
    groups are subsequently added.  This can be used as an
    optimization, to reduce allocation for sets that have a known
    number of intervals.

    @param n_intervals The number of intervals to add.
    @param intervals Array of n_intervals intervals.
  */
  void add_interval_memory(int n_intervals, Interval *intervals);

  /**
    Iterator over intervals for a given SIDNO.

    This is an abstract template class, used as a common base class
    for Const_interval_iterator and Interval_iterator.

    The iterator always points to an interval pointer.  The interval
    pointer is either the initial pointer into the list, or the next
    pointer of one of the intervals in the list.
  */
  template<typename Group_set_t, typename Interval_p> class Interval_iterator_base
  {
  public:
    /**
      Construct a new iterator over the GNO intervals for a given Group_set.

      @param group_set The Group_set.
      @param sidno The SIDNO.
    */
    Interval_iterator_base(Group_set_t *group_set, rpl_sidno sidno)
    {
      DBUG_ASSERT(sidno >= 1 && sidno <= group_set->get_max_sidno());
      init(group_set, sidno);
    }
    /// Construct a new iterator over the free intervals of a Group_set.
    Interval_iterator_base(Group_set_t *group_set)
    { p= &group_set->free_intervals; }
    /// Reset this iterator.
    inline void init(Group_set_t *group_set, rpl_sidno sidno)
    { p= dynamic_element(&group_set->intervals, sidno - 1, Interval_p *); }
    /// Advance current_elem one step.
    inline void next()
    {
      DBUG_ASSERT(*p != NULL);
      p= &(*p)->next;
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
    Iterator over intervals of a const Group_set.
  */
  class Const_interval_iterator
    : public Interval_iterator_base<const Group_set, Interval *const>
  {
  public:
    /// Create this Const_interval_iterator.
    Const_interval_iterator(const Group_set *group_set, rpl_sidno sidno)
      : Interval_iterator_base<const Group_set, Interval *const>(group_set, sidno) {}
    /// Destroy this Const_interval_iterator.
    Const_interval_iterator(const Group_set *group_set)
      : Interval_iterator_base<const Group_set, Interval *const>(group_set) {}
  };

  /**
    Iterator over intervals of a non-const Group_set, with additional
    methods to modify the Group_set.
  */
  class Interval_iterator
    : public Interval_iterator_base<Group_set, Interval *>
  {
  public:
    /// Create this Interval_iterator.
    Interval_iterator(Group_set *group_set, rpl_sidno sidno)
      : Interval_iterator_base<Group_set, Interval *>(group_set, sidno) {}
    /// Destroy this Interval_iterator.
    Interval_iterator(Group_set *group_set)
      : Interval_iterator_base<Group_set, Interval *>(group_set) {}
    /**
      Set current_elem to the given Interval but do not touch the
      next pointer of the given Interval.
    */
    inline void set(Interval *iv) { *p= iv; }
    /// Insert the given element before current_elem.
    inline void insert(Interval *iv) { iv->next= *p; set(iv); }
    /// Remove current_elem.
    inline void remove(Group_set *group_set)
    {
      DBUG_ASSERT(get() != NULL);
      Interval *next= (*p)->next;
      group_set->put_free_interval(*p);
      set(next);
    }
  };


  /**
    Iterator over all groups in a Group_set.  This is a const
    iterator; it does not allow modification of the Group_set.
  */
  class Group_iterator
  {
  public:
    Group_iterator(const Group_set *gs)
      : group_set(gs), sidno(0), ivit(gs) { next_sidno(); }
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
        Interval *iv= ivit.get();
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
    inline Group get() const
    {
      Group ret= { sidno, gno };
      return ret;
    }
  private:
    /// Find the next sidno that has one or more intervals.
    inline void next_sidno()
    {
      Interval *iv;
      do {
        sidno++;
        if (sidno > group_set->get_max_sidno())
        {
          sidno= 0;
          gno= 0;
          return;
        }
        ivit.init(group_set, sidno);
        iv= ivit.get();
      } while (iv == NULL);
      gno= iv->start;
    }
    /// The Group_set we iterate over.
    const Group_set *group_set;
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


private:
  /**
    Contains a list of intervals allocated by this Group_set.  When a
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
  static const int CHUNK_GROW_SIZE = 8;

  /**
    Adds a list of intervals to the given SIDNO.

    The SIDNO must exist in the Group_set before this function is called.

    @param sidno The SIDNO to which intervals will be added.
    @param ivit Iterator over the intervals to add. This is typically
    an iterator over some other Group_set.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status add(rpl_sidno sidno, Const_interval_iterator ivit);
  /**
    Removes a list of intervals to the given SIDNO.

    It is not required that the intervals exist in this Group_set.

    @param sidno The SIDNO from which intervals will be removed.
    @param ivit Iterator over the intervals to remove. This is typically
    an iterator over some other Group_set.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status remove(rpl_sidno sidno, Const_interval_iterator ivit);
  /**
    Adds the interval (start, end) to the given Interval_iterator.

    This is the lowest-level function that adds groups; this is where
    Interval objects are added, grown, or merged.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the interval that
    contains start and end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status add(Interval_iterator *ivitp, rpl_gno start, rpl_gno end);
  /**
    Removes the interval (start, end) from the given
    Interval_iterator. This is the lowest-level function that removes
    groups; this is where Interval objects are removed, truncated, or
    split.

    It is not required that the groups in the interval exist in this
    Group_set.

    @param ivitp Pointer to iterator.  After this function returns,
    the current_element of the iterator will be the next interval
    after end.
    @param start The first GNO in the interval.
    @param end The first GNO after the interval.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status remove(Interval_iterator *ivitp, rpl_gno start, rpl_gno end);
  /**
    Allocates a new chunk of Intervals and adds them to the list of
    unused intervals.

    @param size The number of intervals in this chunk
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status create_new_chunk(int size);
  /**
    Returns a fresh new Interval object.

    This usually does not require any real allocation, it only pops
    the first interval from the list of free intervals.  If there are
    no free intervals, it calls create_new_chunk.

    @param out The resulting Interval* will be stored here.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status get_free_interval(Interval **out);
  /**
    Puts the given interval in the list of free intervals.  Does not
    unlink it from its place in any other list.
  */
  void put_free_interval(Interval *iv);
  /// Worker for the constructor.
  void init(Sid_map *_sid_map, Checkable_rwlock *_sid_lock);

  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Sid_map associated with this Group_set.
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
#ifndef NO_DBUG
  /**
    The number of chunks.  Used only to check some invariants when
    DBUG is on.
  */
  int n_chunks;
#endif

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_SET
  friend FRIEND_OF_GROUP_SET;
#endif
};


/**
  Holds information about a group set.  Can also be NULL.

  This is used as backend storage for @@session.ugid_next_list.  The
  idea is that we allow the user to set this to NULL, but we keep the
  Group_set object so that we can re-use the allocated memory and
  avoid costly allocations later.

  This is stored in struct system_variables (defined in sql_class.h),
  which is cleared using memset(0); hence the negated form of
  is_non_null.

  The convention is: if is_non_null is false, then the value of the
  session variable is NULL, and the field group_set may be NULL or
  non-NULL.  If is_non_null is true, then the value of the session
  variable is not NULL, and the field group_set has to be non-NULL.
*/
struct Group_set_or_null
{
  /// Pointer to the Group_set.
  Group_set *group_set;
  /// True if this Group_set is NULL.
  bool is_non_null;
  /// Return NULL if this is NULL, otherwise return the Group_set.
  inline Group_set *get_group_set() const
  {
    DBUG_ASSERT(!(is_non_null && group_set == NULL));
    return is_non_null ? group_set : NULL;
  }
  /**
    Do nothing if this object is non-null; set to empty set otherwise.

    @return NULL if out of memory; Group_set otherwise.
  */
  Group_set *set_non_null(Sid_map *sm)
  {
    if (!is_non_null)
    {
      if (group_set == NULL)
        group_set= new Group_set(sm);
      else
        group_set->clear();
    }
    is_non_null= (group_set != NULL);
    return group_set;
  }
  /// Set this Group_set to NULL.
  inline void set_null() { is_non_null= false; }
};


/**
  Represents the set of groups that are owned by some thread.

  This consists of all partial groups and a subset of the unlogged
  groups.  Each group has a flag that indicates whether it is partial
  or not.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.

  The internal representation is a DYNAMIC_ARRAY that maps SIDNO to
  HASH, where each HASH maps GNO to (Rpl_owner_id, bool).
*/
class Owned_groups
{
public:
  /**
    Constructs a new, empty Owned_groups object.

    @param sid_lock Read-write lock that protects updates to the
    number of SIDs.
  */
  Owned_groups(Checkable_rwlock *sid_lock);
  /// Destroys this Owned_groups.
  ~Owned_groups();
  /// Mark all owned groups for all SIDs as non-partial.
  void clear();
  /**
    Add a group to this Owned_groups.

    The group will be marked as non-partial, i.e., it has not yet been
    written to the binary log.

    @param sidno The SIDNO of the group to add.
    @param gno The GNO of the group to add.
    @param owner_id The Owner_id of the group to add.

    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status add(rpl_sidno sidno, rpl_gno gno, Rpl_owner_id owner_id);
  /**
    Returns the owner of the given group.

    If the group does not exist in this Owned_groups object, returns
    an Rpl_owner_id object that contains 'no owner'.

    @param sidno The group's SIDNO
    @param gno The group's GNO
    @return Owner of the group.
  */
  Rpl_owner_id get_owner(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Changes owner of the given group.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
    @param owner_id The group's new owner_id.
  */
  void change_owner(rpl_sidno sidno, rpl_gno gno,
                    Rpl_owner_id owner_id) const;
  /**
    Removes the given group.

    If the group does not exist in this Owned_groups object, does
    nothing.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  void remove(rpl_sidno sidno, rpl_gno gno);
  /**
    Marks the given group as partial.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  bool mark_partial(rpl_sidno sidno, rpl_gno gno);
  /**
    Returns true iff the given group is partial.

    Throws an assertion if the group does not exist in this
    Owned_groups object.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
  */
  bool is_partial(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Ensures that this Owned_groups object can accomodate SIDNOs up to
    the given SIDNO.

    If this Owned_groups object needs to be resized, then the lock
    will be temporarily upgraded to a write lock and then degraded to
    a read lock again; there will be a short period when the lock is
    not held at all.

    @param sidno The SIDNO.

    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status ensure_sidno(rpl_sidno sidno);
  /// Returns the maximal sidno that this Owned_groups currently has space for.
  rpl_sidno get_max_sidno() const
  {
    sid_lock->assert_some_lock();
    return sidno_to_hash.elements;
  }
  /**
    Adds all partial groups in this Owned_groups object to the given Group_set.

    @param gs Group_set that will be updated.
    @return GS_SUCCESS or GS_OUT_OF_MEMORY
  */
  enum_group_status get_partial_groups(Group_set *gs) const;

private:
  /// Represents one owned group.
  struct Node
  {
    /// GNO of the group.
    rpl_gno gno;
    /// Owner of the group.
    Rpl_owner_id owner;
    /// If true, this group is partial; i.e., written to the binary log.
    bool is_partial;
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
    not exist in this Owned_groups object.
  */
  Node *get_node(rpl_sidno sidno, rpl_gno gno) const
  {
    return get_node(get_hash(sidno), gno);
  };
  /// Return true iff this Owned_groups object contains the given group.
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const
  {
    return get_node(sidno, gno) != NULL;
  }
  /// Growable array of hashes.
  DYNAMIC_ARRAY sidno_to_hash;
};


/**
  Represents the state of the group log: the set of ended groups and
  the set of owned groups, the owner of each owned group, and a
  Mutex_cond_array that protects updates to groups of each SIDNO.

  This data structure has a read-write lock that protects the number
  of SIDNOs.  The lock is provided by the invoker of the constructor
  and it is generally the caller's responsibility to acquire the read
  lock.  Access methods assert that the caller already holds the read
  (or write) lock.  If a method of this class grows the number of
  SIDNOs, then the method temporarily upgrades this lock to a write
  lock and then degrades it to a read lock again; there will be a
  short period when the lock is not held at all.
*/
class Group_log_state
{
public:
  /**
    Constructs a new Group_log_state object.

    @param _sid_lock Read-write lock that protects updates to the
    number of SIDs.
    @param _sid_map Sid_map used by this group log.
  */
  Group_log_state(Checkable_rwlock *_sid_lock, Sid_map *_sid_map)
    : sid_lock(_sid_lock), sid_locks(_sid_lock),
    sid_map(_sid_map),
    ended_groups(_sid_map), owned_groups(_sid_lock) {}
  /**
    Reset the state after RESET MASTER: remove all ended groups and
    mark all owned groups as non-partial.
  */
  void clear();
  /**
    Returns true if the given group is ended.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @retval true The group is ended in the group log.

    @retval false The group is partial or unlogged in the group log.
  */
  bool is_ended(rpl_sidno sidno, rpl_gno gno) const
  { return ended_groups.contains_group(sidno, gno); }
  /**
    Returns true if the given group is partial.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @retval true The group is partial in the group log.
    @retval false The group is ended or unlogged in the group log.
  */
  bool is_partial(rpl_sidno sidno, rpl_gno gno) const
  { return owned_groups.is_partial(sidno, gno); }
  /**
    Returns the owner of the given group.

    @param sidno The SIDNO to check.
    @param gno The GNO to check.
    @return Rpl_owner_id of the thread that owns the group (possibly
    none, if the group is not owned).
  */
  Rpl_owner_id get_owner(rpl_sidno sidno, rpl_gno gno) const
  { return owned_groups.get_owner(sidno, gno); }
  /**
    Marks the given group as partial.
    
    Raises an assertion if the group is not owned.

    @param sidno The SIDNO of the group.
    @param gno The GNO of the group.
    @return The value of is_partial() before this call.
  */
  bool mark_partial(rpl_sidno sidno, rpl_gno gno)
  { return owned_groups.mark_partial(sidno, gno); }
  /**
    Marks the group as not owned any more.

    If the group is not owned, does nothing.

    @param sidno The SIDNO of the group
    @param gno The GNO of the group.
  */
  /*UNUSED
  void mark_not_owned(rpl_sidno sidno, rpl_gno gno)
  { owned_groups.remove(sidno, gno); }
  */
  /**
    Acquires ownership of the given group, on behalf of the given thread.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.
    @param owner The thread that will own the group.

    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status acquire_ownership(rpl_sidno sidno, rpl_gno gno,
                                      const THD *thd);
  /**
    Ends the given group, i.e., moves it from the set of 'owned
    groups' to the set of 'ended groups'.

    @param sidno The group's SIDNO.
    @param gno The group's GNO.

    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status end_group(rpl_sidno sidno, rpl_gno gno);
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
  /// Waits for updates on the given SIDNO.
  void wait_for_sidno(THD *thd, const Sid_map *sm, Group g, Rpl_owner_id owner);
  /**
    Locks one mutex for each SIDNO where the given Group_set has at
    least one group. If the Group_set is not given, locks all
    mutexes.  Locks are acquired in order of increasing SIDNO.
  */
  void lock_sidnos(const Group_set *set= NULL);
  /**
    Unlocks the mutex for each SIDNO where the given Group_set has at
    least one group.  If the Group_set is not given, unlocks all mutexes.
  */
  void unlock_sidnos(const Group_set *set= NULL);
  /**
    Waits for the condition variable for each SIDNO where the given
    Group_set has at least one group.
  */
  void broadcast_sidnos(const Group_set *set);
  /**
    Ensure that owned_groups, ended_groups, and sid_locks have room
    for at least as many SIDNOs as sid_map.

    Requires that the read lock on sid_locks is held.  If any object
    needs to be resized, then the lock will be temporarily upgraded to
    a write lock and then degraded to a read lock again; there will be
    a short period when the lock is not held at all.

    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status ensure_sidno();
  /// Return a pointer to the Group_set that contains the ended groups.
  const Group_set *get_ended_groups() { return &ended_groups; }
  /// Return a pointer to the Owned_groups that contains the owned groups.
  const Owned_groups *get_owned_groups() { return &owned_groups; }
private:
  /// Read-write lock that protects updates to the number of SIDs.
  mutable Checkable_rwlock *sid_lock;
  /// Contains one mutex/cond pair for every SIDNO.
  Mutex_cond_array sid_locks;
  /// The Sid_map used by this Group_log_state.
  Sid_map *sid_map;
  /// The set of groups that are ended in the group log.
  Group_set ended_groups;
  /// The set of groups that are owned by some thread.
  Owned_groups owned_groups;

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_LOG_STATE
  friend FRIEND_OF_GROUP_LOG_STATE;
#endif
};


/**
  Enumeration of subgroup types.
*/
enum enum_subgroup_type
{
  NORMAL_SUBGROUP, ANONYMOUS_SUBGROUP, DUMMY_SUBGROUP
};


/**
  This struct represents a specification of a UGID for a statement to
  be executed: either "AUTOMATIC", "ANONYMOUS", or "SID:GNO".
*/
struct Ugid_specification
{
  /// The type of group.
  enum enum_type
  {
    AUTOMATIC, ANONYMOUS, UGID, INVALID
  };
  enum_type type;
  /**
    The UGID:
    { SIDNO, GNO } if type == UGID;
    { 0, 0 } if type == AUTOMATIC or ANONYMOUS.
  */
  Group group;
  /**
    Parses the given string and stores in this Ugid_specification.

    @param text The text to parse
    @return GS_SUCCESS or GS_ERROR_PARSE
  */
  enum_group_status parse(const char *text);
  static const int MAX_TEXT_LENGTH= Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH;
  /**
    Writes this Ugid_specification to the given string buffer.

    @param buf The buffer
    @retval buf If the type of this specification: "UUID:NUMBER",
    "ANONYMOUS", or "AUTOMATIC".
  */
  int to_string(char *buf) const;
  /**
    Returns the type of the group, if the given string is a valid Ugid_specification; INVALID otherwise.
  */
  static enum_type get_type(const char *text);
  /// Returns true if the given string is a valid Ugid_specification.
  static bool is_valid(const char *text) { return get_type(text) != INVALID; }
#ifndef NO_DBUG
  void print() const
  {
    char buf[MAX_TEXT_LENGTH + 1];
    to_string(buf);
    printf("%s\n", buf);
  }
#endif
};


/**
   Holds information about a sub-group.

   This can be a normal sub-group, an anonymous sub-group, or a dummy
   sub-group.
*/
struct Subgroup
{
  enum_subgroup_type type;
  rpl_sidno sidno;
  rpl_gno gno;
  rpl_binlog_no binlog_no;
  rpl_binlog_pos binlog_pos;
  rpl_binlog_pos binlog_length;
  rpl_binlog_pos binlog_offset_after_last_statement;
  rpl_lgid lgid;
  bool group_commit;
  bool group_end;
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
  /// Removes all sub-groups from this cache.
  void clear();
  /// Return the number of sub-groups in this group cache.
  inline int get_n_subgroups() const { return subgroups.elements; }
  /// Return true iff the group cache contains zero sub-groups.
  inline bool is_empty() const { return get_n_subgroups() == 0; }
  /**
    Adds a sub-group to this Group_cache.  The sub-group should
    already have been written to the stmt or trx cache.  The SIDNO and
    GNO fields are taken from @@SESSION.UDIG_NEXT.  The GROUP_END
    field is taken from @@SESSION.UGID_END.

    @param thd The THD object from which we read session variables.
    @param binlog_length Length of group in binary log.
  */
  enum_group_status add_logged_subgroup(const THD *thd,
                                        my_off_t binlog_length);
  /**
    Adds a dummy group with the given SIDNO, GNO, and GROUP_END to this cache.

    @param sidno The SIDNO of the group.
    @param gno The GNO of the group.
    @param group_end The GROUP_END of the group.
  */
  enum_group_status add_dummy_subgroup(rpl_sidno sidno, rpl_gno gno,
                                       bool group_end);
  /**
    Add the given group to this cache, unended, unless the cache or
    the Group_log_state already contains it.

    @param gls Group_log_state, used to determine if the group is
    unlogged or not.
    @param sidno The SIDNO of the group to add.
    @param gno The GNO of the group to add.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status add_dummy_subgroup_if_missing(const Group_log_state *gls,
                                                  rpl_sidno sidno, rpl_gno gno);
  /**
    Add all groups in the given Group_set to this cache, unended,
    except groups that exist in this cache or in the Group_log_state.

    @param gls Group_log_state, used to determine if the group is
    unlogged or not.
    @param group_set The set of groups to possibly add.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status add_dummy_subgroups_if_missing(const Group_log_state *gls,
                                                   const Group_set *group_set);
  /**
    Update the binary log's Group_log_state to the state after this
    cache has been flushed.

    @param thd The THD that this Group_log_state belongs to.
    @param gls The binary log's Group_log_state
    @return GS_SUCCESS OR GS_ERROR_OUT_OF_MEMORY
  */
  enum_group_status update_group_log_state(const THD *thd,
                                           Group_log_state *gls) const;
  /**
    Writes all sub-groups in the cache to the group log.

    @todo The group log is not yet implemented. /Sven

    @param trx_group_cache Should be set to the transaction group
    cache. If trx_group_cache is different from 'this', then it is
    assumed that 'this' is the statement group cache.  In that
    case, if a group is ended in 'this' and the group exists in
    trx_group_cache, then the end flag is removed from 'this', and if
    the group is not ended in trx_group_cache, an ending dummy group
    is appended to trx_group_cache.  This operation is necessary to
    prevent sub-groups of the group from being logged after ended
    sub-groups of the group.
  */
  enum_group_status write_to_log(Group_cache *trx_group_cache,
                                 rpl_binlog_pos offset_after_last_statement);
  /**
    Generates GNO for all groups that are committed for the first time
    in this Group_cache.

    This acquires ownership of all groups.  After this call, this
    Group_cache does not contain any Cached_subgroups that have
    type==NORMAL_SUBGROUP and gno<=0.

    @param thd The THD that this Group_log_state belongs to.
    @param gls The Group_log_state where group ownership is acquired.
  */
  void generate_automatic_gno(const THD *thd, Group_log_state *gls);
  /**
    Return true if this Group_cache contains the given group.

    @param sidno The SIDNO of the group to check.
    @param gno The GNO of the group to check.
    @retval true The group exists in this cache.
    @retval false The group does not exist in this cache.
  */
  bool contains_group(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Return true if the given group is ended in this Group_cache.

    @param sidno SIDNO of the group to check.
    @param gno GNO of the group to check.
    @retval true The group is ended in this Group_cache.
    @retval false The group is not ended in this Group_cache.
  */
  bool group_is_ended(rpl_sidno sidno, rpl_gno gno) const;
  /**
    Add all groups that exist but are unended in this Group_cache to the given Group_set.

    If this Owned_groups contains SIDNOs that do not exist in the
    Group_set, then the Group_set's array of lists of intervals will
    be grown.  If the Group_set has a sid_lock, then the method
    temporarily upgrades the lock to a write lock and then degrades it
    to a read lock again; there will be a short period when the lock
    is not held at all.

    @param gs The Group_set to which groups are added.
    @return GS_SUCCESS or GS_OUT_OF_MEMORY
  */
  enum_group_status get_partial_groups(Group_set *gs) const;
  /**
    Add all groups that exist and are ended in this Group_cache to the given Group_set.

    @param gs The Group_set to which groups are added.
    @return GS_SUCCESS or GS_OUT_OF_MEMORY
  */
  enum_group_status get_ended_groups(Group_set *gs) const;

#ifndef NO_DBUG
  void get_string(Sid_map *sm, char *buf)
  {
    int n_subgroups= get_n_subgroups();

    buf += sprintf(buf, "%d sub-groups = {\n", n_subgroups);
    for (int i= 0; i < n_subgroups; i++)
    {
      Cached_subgroup *cs= get_unsafe_pointer(i);
      char uuid[Uuid::TEXT_LENGTH + 1]= "[]";
      if (cs->sidno)
        sm->sidno_to_sid(cs->sidno)->to_string(uuid);
      buf +=
        sprintf(buf, "  %s:%lld%s [%lld bytes] - %s\n",
                uuid, cs->gno, cs->group_end ? "-END":"", cs->binlog_length,
                cs->type == NORMAL_SUBGROUP ? "NORMAL" :
                cs->type == ANONYMOUS_SUBGROUP ? "ANON" :
                cs->type == DUMMY_SUBGROUP ? "DUMMY" :
                "INVALID-SUBGROUP-TYPE");
    }
    sprintf(buf, "}\n");
  }
  size_t get_string_length()
  {
    return (2 + Uuid::TEXT_LENGTH + 1 + MAX_GNO_TEXT_LENGTH + 4 + 2 +
            40 + 10 + 21 + 1 + 100/*margin*/) * get_n_subgroups() + 100/*margin*/;
  }
  char *get_string(Sid_map *sm)
  {
    char *buf= (char *)malloc(get_string_length());
    get_string(sm, buf);
    return buf;
  }
#endif

private:
  /**
    Represents a sub-group in the group cache.

    Groups in the group cache are slightly different from other
    sub-groups, because not all information about them is known.

    Automatic sub-groups are marked as such by setting gno<=0.
  */
  struct Cached_subgroup
  {
    enum_subgroup_type type;
    rpl_sidno sidno;
    rpl_gno gno;
    rpl_binlog_pos binlog_length;
    bool group_end;
  };

  /// List of all subgroups in this cache, of type Cached_subgroup.
  DYNAMIC_ARRAY subgroups;

  /**
    Returns a pointer to the given subgroup.  The pointer is only
    valid until the next time a sub-group is added or removed.

    @param index Index of the element: 0 <= index < get_n_subgroups().
  */
  inline Cached_subgroup *get_unsafe_pointer(int index) const
  {
    DBUG_ASSERT(index >= 0 && index < get_n_subgroups());
    return dynamic_element(&subgroups, index, Cached_subgroup *);
  }
  /**
    Adds the given sub-group to this group cache, or merges it with the
    last existing sub-group in the cache if they are compatible.

    @param subgroup The subgroup to add.
    @return GS_SUCCESS or GS_ERROR_OUT_OF_MEMORY.
  */
  enum_group_status add_subgroup(const Cached_subgroup *subgroup);
  /**
    Prepare the cache to be written to the group log.

    @todo The group log is not yet implemented. /Sven

    @param trx_group_cache @see write_to_log.
  */
  enum_group_status
    write_to_log_prepare(Group_cache *trx_group_cache,
                         rpl_binlog_pos offset_after_last_statement,
                         Cached_subgroup **last_non_dummy_subgroup);

  /// Used by unit tests that need to access private members.
#ifdef FRIEND_OF_GROUP_CACHE
  friend FRIEND_OF_GROUP_CACHE;
#endif
};


/**
  Represents a bidirectional map between binlog filenames and
  binlog_no.
*/
class Binlog_map
{
public:
  rpl_binlog_no filename_to_binlog_no(const char *filename) const;
  void binlog_no_to_filename(rpl_sid sid, char *buf) const;
private:
  rpl_binlog_no number_offset;
  DYNAMIC_ARRAY binlog_no_to_filename_map;
  HASH filename_to_binlog_no_map;
};


/**
  Indicates if a statement should be skipped or not. Used as return
  value from ugid_before_statement.
*/
enum enum_ugid_statement_status
{
  /// Statement can execute.
  UGID_STATEMENT_EXECUTE,
  /// Statement should be cancelled.
  UGID_STATEMENT_CANCEL,
  /**
    Statement should be skipped, but there may be an implicit commit
    after the statement if ugid_commit is set.
  */
  UGID_STATEMENT_SKIP
};
/**
  Before the transaction cache is flushed, this function checks if we
  need to add an ending dummy groups sub-groups.
*/
enum_ugid_statement_status
ugid_before_statement(THD *thd, Checkable_rwlock *lock,
                      Group_log_state *gls,
                      Group_cache *gsc, Group_cache *gtc);
int ugid_before_flush_trx_cache(THD *thd, Checkable_rwlock *lock,
                                Group_log_state *gls, Group_cache *gc);
int ugid_flush_group_cache(THD *thd, Checkable_rwlock *lock,
                           Group_log_state *gls,
                           Group_cache *gc, Group_cache *trx_cache,
                           rpl_binlog_pos offset_after_last_statement);


class Atom_file
{
public:
  Atom_file(const char *file_name);
  int open(bool write);
  int close();
  bool is_open() const { return fd != -1; }
  bool is_writable() const { return fd != -1 && writable; }
  size_t append(my_off_t length, const uchar *data)
  {
    DBUG_ENTER("Atom_file::append");
    DBUG_ASSERT(is_writable());
    my_off_t ret= my_write(fd, data, length, MYF(MY_WME));
    DBUG_RETURN(ret);
  }
  size_t pread(my_off_t offset, my_off_t length, uchar *buffer) const;
  int truncate_and_append(my_off_t offset, my_off_t length, const uchar *data);
  int sync()
  {
    DBUG_ASSERT(is_writable());
    return my_sync(fd, MYF(MY_WME));
  }
  ~Atom_file() { DBUG_ASSERT(!is_open()); }
private:
  static const int HEADER_LENGTH= 9;
  int rollback();
  int commit(my_off_t offset, my_off_t length);
  int recover();
  static const char *OVERWRITE_FILE_SUFFIX;
  char file_name[FN_REFLEN];
  char overwrite_file_name[FN_REFLEN];
  File fd;
  bool writable;
  File ofd;
  my_off_t overwrite_offset;
};


class Rot_file
{
public:
  Rot_file(const char *file_name);
  int open(bool write);
  int close();
  my_off_t append(my_off_t length, const uchar *data)
  {
    DBUG_ENTER("Rot_file::append");
    DBUG_ASSERT(is_writable());
    my_off_t ret= my_write(fd, data, length, MYF(MY_WME));
    DBUG_RETURN(ret);
  }
  my_off_t pread(my_off_t offset, my_off_t length, uchar *buffer) const;
  void set_rotation_limit(my_off_t limit) { rotation_limit= limit; }
  my_off_t get_rotation_limit() const { return rotation_limit; }
  int purge(my_off_t offset);
  int truncate(my_off_t offset);
  int sync()
  {
    DBUG_ASSERT(is_writable());
    return my_sync(fd, MYF(MY_WME));
  }
  bool is_writable() const { return writable; }
  bool is_open() const { return fd != -1; }
  ~Rot_file();
private:
  int header_length;
  char file_name[FN_REFLEN];
  File fd;
  my_off_t rotation_limit;
  bool writable;
/*
  struct Sub_file
  {
    int fd;
    my_off_t offset;
    int index;
  };
  enum enum_state { CLOSED, OPEN_READ, OPEN_READ_WRITE };
  char file_name[FN_REFLEN];
  int fd;
  enum_state state;
  my_off_t limit;
*/
};


class Group_log
{
public:
  Group_log(const char *filename) : rot_file(filename) {}

  int write_subgroup(const Subgroup *subgroup);
  class Read_iterator
  {
  public:
    Read_iterator(Group_set group_set);
    int read(Subroup *subgroup);
  private:
    my_off_t offset;
    Read_state read_state;
  };

private:
  class Read_state
  {
    rpl_lgid lgid;
  };
  Read_state read_state;
  Rot_file group_log_file;
  static const int write_buf_size= 0x10000;
  uchar write_buf[WRITE_BUF_SIZE];
  uchar *write_buf_pos;
};


/**
  Auxiliary class for reading and writing compact-encoded numbers to
  file.
*/
class Compact_encoding
{
public:
  /**
    Write a compact-encoded unsigned integer to the given file.

    @param fd File to write to.
    @param n Number to write.
    @param myf MYF() flags.

    @return On success, returns number of bytes written (1...10).  On
    failure, returns a number <= 0.
  */
  static int write_unsigned(File fd, ulonglong n, myf my_flags);
  /**
    Read a compact-encoded unsigned integer from the given file.
    
    @param fd File to write to.
    @param out Number will be stored in this parameter.
    @param myf MYF() flags.

    @return On success, returns the number of bytes read (1...10).  On
    read error, returns the negative number of bytes read, (-10...0).
    If the number is malformed, returns -0x10000 - number of bytes
    read.
  */
  static int read_unsigned(File fd, ulonglong *out, myf my_flags);
  /**
    Write a compact-encoded signed integer to the given file.
    @see write_unsigned.
  */
  static int write_signed(File fd, longlong n, myf my_flags);
  /**
    Read a compact-encoded signed integer from the given file.
    @see read_unsigned.
  */
  static int read_signed(File fd, longlong *out, myf my_flags);
};


#endif /* HAVE_UGID */

#endif /* RPL_GROUPS_H_INCLUDED */
