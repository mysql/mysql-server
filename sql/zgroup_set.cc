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


#include "zgroups.h"


#ifdef HAVE_UGID


#include <ctype.h>
#include "mysqld.h"
#include "my_dbug.h"


const int Group_set::CHUNK_GROW_SIZE;


const Group_set::String_format Group_set::default_string_format=
{
  "", "", ":", "-", ":", ",\n",
  0, 0, 1, 1, 1, 2
};


const Group_set::String_format Group_set::sql_string_format=
{
  "'", "'", ":", "-", ":", "',\n'",
  1, 1, 1, 1, 1, 4
};


Group_set::Group_set(Sid_map *_sid_map, Checkable_rwlock *_sid_lock)
{
  init(_sid_map, _sid_lock);
}


Group_set::Group_set(Sid_map *_sid_map, const char *text,
                     enum_group_status *status, Checkable_rwlock *_sid_lock)
{
  init(_sid_map, _sid_lock);
  *status= add(text);
}


Group_set::Group_set(Group_set *other, enum_group_status *status)
{
  init(other->sid_map, other->sid_lock);
  *status= add(other);
}


void Group_set::init(Sid_map *_sid_map, Checkable_rwlock *_sid_lock)
{
  DBUG_ENTER("Group_set::init");
  sid_map= _sid_map;
  sid_lock= _sid_lock;
  cached_string_length= -1;
  cached_string_format= NULL;
  chunks= NULL;
  free_intervals= NULL;
  my_init_dynamic_array(&intervals, sizeof(Interval *), 0, 8);
  //GROUP_STATUS_THROW(ensure_sidno(sid_map->get_max_sidno()));
#ifndef NO_DBUG
  n_chunks= 0;
#endif
  DBUG_VOID_RETURN;
}


Group_set::~Group_set()
{
  DBUG_ENTER("Group_set::~Group_set");
  Interval_chunk *chunk= chunks;
  while (chunk != NULL)
  {
    Interval_chunk *next_chunk= chunk->next;
    free(chunk);
    chunk= next_chunk;
#ifndef NO_DEBUG
    n_chunks--;
#endif
  }
  DBUG_ASSERT(n_chunks == 0);
  delete_dynamic(&intervals);
  DBUG_VOID_RETURN;
}


enum_group_status Group_set::ensure_sidno(rpl_sidno sidno)
{
  DBUG_ENTER("Group_set::ensure_sidno");
  if (sid_lock != NULL)
    sid_lock->assert_some_rdlock();
  DBUG_ASSERT(sidno <= sid_map->get_max_sidno());
  rpl_sidno max_sidno= get_max_sidno();
  if (sidno > max_sidno)
  {
    /*
      Not all Group_sets are protected by an rwlock.  But if this
      Group_set is, we assume that the read lock has been taken.
      Then we temporarily upgrade it to a write lock while resizing
      the array, and then we restore it to a read lock at the end.
    */
    if (sid_lock != NULL)
    {
      sid_lock->unlock();
      sid_lock->wrlock();
      if (sidno <= max_sidno)
      {
        sid_lock->unlock();
        sid_lock->rdlock();
        DBUG_RETURN(GS_SUCCESS);
      }
    }
    allocate_dynamic(&intervals, sid_map->get_max_sidno());
    Interval *null_p= NULL;
    for (rpl_sidno i= max_sidno; i < sidno; i++)
      insert_dynamic(&intervals, &null_p);
    if (sid_lock != NULL)
    {
      sid_lock->unlock();
      sid_lock->rdlock();
    }
  }
  DBUG_RETURN(GS_SUCCESS);
}


void Group_set::add_interval_memory(int n_ivs, Interval *ivs)
{
  DBUG_ENTER("Group_set::add_interval_memory");
  // make ivs a linked list
  for (int i= 0; i < n_ivs - 1; i++)
    ivs[i].next= &(ivs[i + 1]);
  Interval_iterator ivit(this);
  ivs[n_ivs - 1].next= ivit.get();
  // add intervals to list of free intervals
  ivit.set(&(ivs[0]));
  DBUG_VOID_RETURN;
}


enum_group_status Group_set::create_new_chunk(int size)
{
  DBUG_ENTER("Group_set::create_new_chunk");
  // allocate the new chunk. one element is already pre-allocated, so
  // we only add size-1 elements to the size of the struct.
  Interval_chunk *new_chunk=
    (Interval_chunk *)malloc(sizeof(Interval_chunk) +
                             sizeof(Interval) * (size - 1));
  if (new_chunk == NULL)
    DBUG_RETURN(GS_ERROR_OUT_OF_MEMORY);
  // store the chunk in the list of chunks
  new_chunk->next= chunks;
  chunks= new_chunk;
#ifndef NO_DBUG
  n_chunks++;
#endif
  // add the intervals in the chunk to the list of free intervals
  add_interval_memory(size, new_chunk->intervals);
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_set::get_free_interval(Interval **out)
{
  DBUG_ENTER("Group_set::get_free_interval");
  Interval_iterator ivit(this);
  if (ivit.get() == NULL)
    GROUP_STATUS_THROW(create_new_chunk(CHUNK_GROW_SIZE));
  *out= ivit.get();
  ivit.set((*out)->next);
  DBUG_RETURN(GS_SUCCESS);
}


void Group_set::put_free_interval(Interval *iv)
{
  DBUG_ENTER("Group_set::put_free_interval");
  Interval_iterator ivit(this);
  iv->next= ivit.get();
  ivit.set(iv);
  DBUG_VOID_RETURN;
}


void Group_set::clear()
{
  DBUG_ENTER("Group_set::clear");
  rpl_sidno max_sidno= get_max_sidno();
  if (max_sidno == 0)
    DBUG_VOID_RETURN;
  Interval_iterator free_ivit(this);
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
  {
    /*
      Link in this list of intervals at the end of the list of
      free intervals.
    */
    Interval_iterator ivit(this, sidno);
    Interval *iv= ivit.get();
    if (iv != NULL)
    {
      // find the end of the list of free intervals
      while (free_ivit.get() != NULL)
        free_ivit.next();
      // append the present list
      free_ivit.set(iv);
      // clear the pointer to the head of this list
      ivit.set(NULL);
    }
  }
  DBUG_VOID_RETURN;
}


enum_group_status Group_set::add(Interval_iterator *ivitp,
                                 rpl_gno start, rpl_gno end)
{
  DBUG_ENTER("Group_set::add(Interval_iterator*, rpl_gno, rpl_gno)");
  DBUG_ASSERT(start < end);
  Interval *iv;
  Interval_iterator ivit= *ivitp;
  cached_string_length= -1;

  while ((iv= ivit.get()) != NULL)
  {
    if (start <= iv->end)
    {
      if (end < iv->start)
        // (start, end) is strictly before the current interval
        break;
      // (start, end) and (iv->start, iv->end) touch or intersect.
      // Save the start of the merged interval.
      if (iv->start < start)
        start= iv->start;
      // Remove the current interval as long as the new interval
      // intersects with the next interval.
      while (iv->next && end >= iv->next->start)
      {
        ivit.remove(this);
        iv= ivit.get();
      }
      // Store the interval in the current interval.
      iv->start= start;
      if (end > iv->end)
        iv->end= end;
      *ivitp= ivit;
      DBUG_RETURN(GS_SUCCESS);
    }
    ivit.next();
  }
  /*
    We come here if the interval cannot be combined with any existing
    interval: it is after the previous interval (if any) and before
    the current interval (if any). So we allocate a new interval and
    insert it at the current position.
  */
  Interval *new_iv;
  GROUP_STATUS_THROW(get_free_interval(&new_iv));
  new_iv->start= start;
  new_iv->end= end;
  ivit.insert(new_iv);
  *ivitp= ivit;
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_set::add(rpl_sidno sidno, rpl_gno start, rpl_gno end)
{
  DBUG_ENTER("Group_set::add(rpl_sidno, rpl_gno, rpl_gno)");
  DBUG_ASSERT(sidno >= 1 && start > 0 && end > start);
  ensure_sidno(sidno);
  Interval_iterator ivit(this, sidno);
  DBUG_RETURN(add(&ivit, start, end));
}


rpl_gno parse_gno(const char **s)
{
  char *endp;
  rpl_gno ret= strtoll(*s, &endp, 0);
  if (ret <= 0 || ret == LLONG_MAX)
    return 0;
  *s= endp;
  return ret;
}


int format_gno(char *s, rpl_gno gno)
{
  return ll2str(gno, s, 10, 1) - s;
}


enum_group_status Group_set::add(const char *text)
{
#define SKIP_WHITESPACE() while (isspace(*s)) s++
  DBUG_ENTER("Group_set::add(const char*)");
  const char *s= text;

  SKIP_WHITESPACE();
  if (*s == 0)
    DBUG_RETURN(GS_SUCCESS);

  // Allocate space for all intervals at once, if nothing is allocated.
  if (chunks == NULL)
  {
    // compute number of intervals in text: it is equal to the number of
    // colons
    int n_intervals= 0;
    text= s;
    for (; *s; s++)
      if (*s == ':')
        n_intervals++;
    // allocate all intervals in one chunk
    create_new_chunk(n_intervals);
    s= text;
  }

  do
  {
    // Skip commas (we allow empty SID:GNO specifications).
    while (*s == ',')
    {
      s++;
      SKIP_WHITESPACE();
    }

    // Parse SID.
    rpl_sid sid;
    GROUP_STATUS_THROW(sid.parse(s));
    s += rpl_sid::TEXT_LENGTH;
    rpl_sidno sidno= sid_map->add_permanent(&sid, 0);
    if (sidno <= 0)
      DBUG_RETURN((enum_group_status)sidno);
    GROUP_STATUS_THROW(ensure_sidno(sidno));
    SKIP_WHITESPACE();

    // Iterate over intervals.
    Interval_iterator ivit(this, sidno);
    while (*s == ':')
    {
      // Skip ':'.
      s++;

      // Read start of interval.
      rpl_gno start= parse_gno(&s);
      if (start == 0)
        DBUG_RETURN(GS_ERROR_PARSE);
      SKIP_WHITESPACE();

      // Read end of interval
      rpl_gno end;
      if (*s == '-')
      {
        s++;
        end= parse_gno(&s);
        if (end == 0)
          DBUG_RETURN(GS_ERROR_PARSE);
        end++;
        SKIP_WHITESPACE();
      }
      else
        end= start + 1;

      // Add interval.  Use the existing iterator position if the
      // current interval does not begin before it.  Otherwise iterate
      // from the beginning.
      Interval *current= ivit.get();
      if (current != NULL && start >= current->start)
        GROUP_STATUS_THROW(add(&ivit, start, end));
      else
        GROUP_STATUS_THROW(add(sidno, start, end));
    }
  } while (*s == ',');
  if (*s != 0)
    DBUG_RETURN(GS_ERROR_PARSE);
  DBUG_RETURN(GS_SUCCESS);
}


bool Group_set::is_valid(const char *text)
{
  DBUG_ENTER("Group_set::is_valid(const char*)");
  const char *s= text;

  SKIP_WHITESPACE();
  if (*s == 0)
    DBUG_RETURN(true);
  do
  {
    // Skip commas (we allow empty SID:GNO specifications).
    while (*s == ',')
    {
      s++;
      SKIP_WHITESPACE();
    }

    // Parse SID.
    if (!rpl_sid::is_valid(s))
      DBUG_RETURN(false);
    s += rpl_sid::TEXT_LENGTH;
    SKIP_WHITESPACE();

    // Iterate over intervals.
    while (*s == ':')
    {
      // Skip ':'.
      s++;

      // Read start of interval.
      if (parse_gno(&s) == 0)
        DBUG_RETURN(false);
      SKIP_WHITESPACE();

      // Read end of interval
      if (*s == '-')
      {
        s++;
        if (parse_gno(&s) == 0)
          DBUG_RETURN(false);
        SKIP_WHITESPACE();
      }
    }
  } while (*s == ',');
  if (*s != 0)
    DBUG_RETURN(false);
  DBUG_RETURN(true);
}


enum_group_status Group_set::add(rpl_sidno sidno, Const_interval_iterator other_ivit)
{
  DBUG_ENTER("Group_set::add(rpl_sidno, Interval_iterator)");
  DBUG_ASSERT(sidno >= 1 && sidno <= get_max_sidno());
  Interval *iv;
  Interval_iterator ivit(this, sidno);
  while ((iv= other_ivit.get()) != NULL)
  {
    GROUP_STATUS_THROW(add(&ivit, iv->start, iv->end));
    other_ivit.next();
  }
  DBUG_RETURN(GS_SUCCESS);
}


enum_group_status Group_set::add(const Group_set *other)
{
  DBUG_ENTER("Group_set::add(Group_set *)");
  rpl_sidno max_other_sidno= other->get_max_sidno();
  if (other->sid_map == sid_map)
  {
    GROUP_STATUS_THROW(ensure_sidno(max_other_sidno));
    for (rpl_sidno sidno= 1; sidno <= max_other_sidno; sidno++)
      GROUP_STATUS_THROW(add(sidno, Const_interval_iterator(other, sidno)));
  }
  else
  {
    Sid_map *other_sid_map= other->sid_map;
    for (rpl_sidno other_sidno= 1; other_sidno <= max_other_sidno;
         other_sidno++)
    {
      Const_interval_iterator other_ivit(other, other_sidno);
      if (other_ivit.get() != NULL)
      {
        const rpl_sid *sid= other_sid_map->sidno_to_sid(other_sidno);
        rpl_sidno this_sidno= sid_map->add_permanent(sid);
        if (this_sidno == 0)/// @todo: can also be io error /sven
          DBUG_RETURN(GS_ERROR_OUT_OF_MEMORY);
        ensure_sidno(this_sidno);
        GROUP_STATUS_THROW(add(this_sidno, other_ivit));
      }
    }
  }
  DBUG_RETURN(GS_SUCCESS);
}


bool Group_set::contains_group(rpl_sidno sidno, rpl_gno gno) const
{
  DBUG_ENTER("Group_set::contains_group");
  DBUG_ASSERT(sidno >= 1 && gno >= 1);
  if (sidno > get_max_sidno())
    DBUG_RETURN(false);
  Const_interval_iterator ivit(this, sidno);
  const Interval *iv;
  while ((iv= ivit.get()) != NULL)
  {
    if (gno < iv->start)
      DBUG_RETURN(false);
    else if (gno < iv->end)
      DBUG_RETURN(true);
    ivit.next();
  }
  DBUG_RETURN(false);
}


int Group_set::to_string(char *buf, const Group_set::String_format *sf) const
{
  DBUG_ENTER("Group_set::to_string");
  rpl_sidno map_max_sidno= sid_map->get_max_sidno();
  memcpy(buf, sf->begin, sf->begin_length);
  char *s= buf + sf->begin_length;
  bool first_sidno= true;
  for (int i= 0; i < map_max_sidno; i++)
  {
    rpl_sidno sidno= sid_map->get_sorted_sidno(i);
    if (contains_sidno(sidno))
    {
      Const_interval_iterator ivit(this, sidno);
      const Interval *iv= ivit.get();
      if (first_sidno)
        first_sidno= false;
      else
      {
        memcpy(s, sf->gno_sid_separator, sf->gno_sid_separator_length);
        s+= sf->gno_sid_separator_length;
      }
      s+= sid_map->sidno_to_sid(sidno)->to_string(s);
      bool first_gno= true;
      do {
        if (first_gno)
        {
          memcpy(s, sf->sid_gno_separator, sf->sid_gno_separator_length);
          s+= sf->sid_gno_separator_length;
        }
        else
        {
          memcpy(s, sf->gno_gno_separator, sf->gno_gno_separator_length);
          s+= sf->gno_gno_separator_length;
        }
        s+= format_gno(s, iv->start);
        if (iv->end > iv->start + 1)
        {
          memcpy(s, sf->gno_start_end_separator,
                 sf->gno_start_end_separator_length);
          s+= sf->gno_start_end_separator_length;
          s+= format_gno(s, iv->end - 1);
        }
        ivit.next();
        iv= ivit.get();
      } while (iv != NULL);
    }
  }
  memcpy(s, sf->end, sf->end_length);
  s += sf->end_length;
  *s= 0;
  DBUG_ASSERT(s - buf == get_string_length());
  DBUG_RETURN(s - buf);
}


/**
  Returns the length that the given rpl_sidno (64 bit integer) would
  have, if it was encoded as a string.
*/
static int get_string_length(rpl_gno gno)
{
  DBUG_ASSERT(gno >= 1 && gno < MAX_GNO);
  rpl_gno cmp, cmp2;
  int len= 1;
  if (gno >= 10000000000000000LL)
    len+= 16, cmp= 10000000000000000LL;
  else
  {
    if (gno >= 100000000LL)
      len += 8, cmp = 100000000LL;
    else
      cmp= 1;
    cmp2= cmp * 10000LL;
    if (gno >= cmp2)
      len += 4, cmp = cmp2;
  }
  cmp2= cmp * 100LL;
  if (gno >= cmp2)
    len += 2, cmp = cmp2;
  cmp2= cmp * 10LL;
  if (gno >= cmp2)
    len++;
#ifndef DBUG_OFF
  char buf[22];
  DBUG_ASSERT(snprintf(buf, 22, "%lld", gno) == len);
#endif
  return len;
}


int Group_set::get_string_length(const Group_set::String_format *sf) const
{
  if (cached_string_length == -1 || cached_string_format != sf)
  {
    int n_sids= 0, n_intervals= 0, n_long_intervals= 0;
    int total_interval_length= 0;
    rpl_sidno max_sidno= get_max_sidno();
    for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
    {
      Const_interval_iterator ivit(this, sidno);
      const Interval *iv= ivit.get();
      if (iv != NULL)
      {
        n_sids++;
        do {
          n_intervals++;
          total_interval_length += ::get_string_length(iv->start);
          if (iv->end - 1 > iv->start)
          {
            n_long_intervals++;
            total_interval_length += ::get_string_length(iv->end - 1);
          }
          ivit.next();
          iv= ivit.get();
        } while (iv != NULL);
      }
    }
    cached_string_length= sf->begin_length + sf->end_length;
    if (n_sids > 0)
      cached_string_length+=
        total_interval_length +
        n_sids * (rpl_sid::TEXT_LENGTH + sf->sid_gno_separator_length) +
        (n_sids - 1) * sf->gno_sid_separator_length +
        (n_intervals - n_sids) * sf->gno_gno_separator_length +
        n_long_intervals * sf->gno_start_end_separator_length;
    cached_string_format= sf;
  }
  return cached_string_length;
}


bool Group_set::equals(const Group_set *other) const
{
  DBUG_ENTER("Group_set::equals");
  Sid_map *other_sid_map= other->sid_map;
  rpl_sidno map_max_sidno= sid_map->get_max_sidno();
  rpl_sidno other_map_max_sidno= other_sid_map->get_max_sidno();

  int sid_i= 0, other_sid_i= 0;
  while (1)
  {
    rpl_sidno sidno, other_sidno;
    // find next sidno (in order of increasing sid) for this set
    while (sid_i < map_max_sidno &&
           !contains_sidno(sidno= sid_map->get_sorted_sidno(sid_i)))
      sid_i++;
    // find next sidno (in order of increasing sid) for other set
    while (other_sid_i < other_map_max_sidno &&
           !other->contains_sidno(other_sidno=
                                  other_sid_map->get_sorted_sidno(other_sid_i)))
      other_sid_i++;
    // at least one of this and other reached the max sidno
    if (sid_i == map_max_sidno || other_sid_i == other_map_max_sidno)
      // return true iff both sets reached the max sidno
      DBUG_RETURN(sid_i == map_max_sidno && other_sid_i == other_map_max_sidno);
    // check if sids are equal
    const rpl_sid *sid= sid_map->sidno_to_sid(sidno);
    const rpl_sid *other_sid= other_sid_map->sidno_to_sid(other_sidno);
    if (!sid->equals(other_sid))
      DBUG_RETURN(false);
    // check if all intervals are equal
    Const_interval_iterator ivit(this, sidno);
    Const_interval_iterator other_ivit(other, other_sidno);
    const Interval *iv= ivit.get();
    const Interval *other_iv= other_ivit.get();
    do {
      if (!iv->equals(other_iv))
        DBUG_RETURN(false);
      ivit.next();
      other_ivit.next();
      iv= ivit.get();
      other_iv= other_ivit.get();
    } while (iv != NULL && other_iv != NULL);
    if (iv != NULL || other_iv != NULL)
      DBUG_RETURN(false);
    sid_i++;
    other_sid_i++;
  }
  DBUG_ASSERT(0); // not reached
  DBUG_RETURN(true);
}


bool Group_set::is_subset(const Group_set *super) const
{
  DBUG_ENTER("Group_set::is_subset");
  Sid_map *super_sid_map= super->sid_map;
  rpl_sidno map_max_sidno= sid_map->get_max_sidno();

  int sidno= 0, super_sidno= 0;
  Const_interval_iterator ivit(this), super_ivit(super);
  const Interval *iv, *super_iv;
  while (1)
  {
    // Find the next sidno that has one or more Intervals in this Group_set.
    do
    {
      sidno++;
      if (sidno > map_max_sidno)
        DBUG_RETURN(true);
      ivit.init(this, sidno);
      iv= ivit.get();
    } while (iv == NULL);
    // get corresponding super_sidno
    if (super_sid_map == sid_map)
      super_sidno= sidno;
    else
    {
      super_sidno= super_sid_map->sid_to_sidno(sid_map->sidno_to_sid(sidno));
      if (super_sidno == 0)
        DBUG_RETURN(false);
    }
    super_ivit.init(super, super_sidno);
    super_iv= super_ivit.get();
    // check if all intervals for this sidno are contained in some
    // interval of super
    do {
      if (super_iv == NULL)
        DBUG_RETURN(false);
      while (iv->start > super_iv->end)
      {
        super_ivit.next();
        super_iv= super_ivit.get();
      }
      if (iv->start < super_iv->start || iv->end > super_iv->end)
        DBUG_RETURN(false);
      ivit.next();
      iv= ivit.get();
    } while (iv != NULL);
  }
  DBUG_ASSERT(0); // not reached
  DBUG_RETURN(true);
}


#endif /* HAVE_UGID */
