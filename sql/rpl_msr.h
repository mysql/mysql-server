/* Copyright (c) 2014, 2021, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#ifndef RPL_MSR_H
#define RPL_MSR_H

#ifdef HAVE_REPLICATION

#include "my_global.h"
#include "rpl_channel_service_interface.h" // enum_channel_type
#include "rpl_mi.h"                        // Master_info

#include <map>
#include <string>


/**
   Maps a channel name to it's Master_info.
*/

//Maps a master info object to a channel name
typedef std::map<std::string, Master_info*> mi_map;
//Maps a channel type to a map of channels of that type.
typedef std::map<int, mi_map> replication_channel_map;

/**
  Class to store all the Master_info objects of a slave
  to access them in the replication code base or performance
  schema replication tables.

  In a Multisourced replication setup, a slave connects
  to several masters (also called as sources). This class
  stores the Master_infos where each Master_info belongs
  to a slave.

  The important objects for a slave are the following:
  i) Master_info and Relay_log_info (slave_parallel_workers == 0)
  ii) Master_info, Relay_log_info and Slave_worker(slave_parallel_workers >0 )

  Master_info is always assosiated with a Relay_log_info per channel.
  So, it is enough to store Master_infos and call the corresponding
  Relay_log_info by mi->rli;

  This class is not yet thread safe. Any part of replication code that
  calls this class member function should always lock the channel_map.

  Only a single global object for a server instance should be created.

  The two important data structures in this class are
  i) C++ std map to store the Master_info pointers with channel name as a key.
    These are the base channel maps.
    @TODO: convert to boost after it's introduction.
  ii) C++ std map to store the channel maps with a channel type as its key.
      This map stores slave channel maps, group replication channels or others
  iii) An array of Master_info pointers to access from performance schema
     tables. This array is specifically implemented in a way to make
      a) pfs indices simple i.e a simple integer counter
      b) To avoid recalibration of data structure if master info is deleted.
         * Consider the following high level implementation of a pfs table
            to make a row.
          <pseudo_code>
          highlevel_pfs_funciton()
          {
           while(replication_table_xxxx.rnd_next())
           {
             do stuff;
           }
          }
         </pseudo_code>
         However, we lock channel_map lock for every rnd_next(); There is a gap
         where an addition/deletion of a channel would rearrange the map
         making the integer indices of the pfs table point to a wrong value.
         Either missing a row or duplicating a row.

         We solve this problem, by using an array exclusively to use in
         replciation pfs tables, by marking a master_info defeated as 0
         (i.e NULL). A new master info is added to this array at the
         first NULL always.
*/
class Multisource_info
{

private:
 /* Maximum number of channels per slave */
  static const unsigned int MAX_CHANNELS= 256;

  /* A Map that maps, a channel name to a Master_info grouped by channel type */
  replication_channel_map rep_channel_map;

  /* Number of master_infos at the moment*/
  uint current_mi_count;

  /**
    Default_channel for this instance, currently is predefined
    and cannot be modified.
  */
  static const char* default_channel;
  Master_info *default_channel_mi;
  static const char* group_replication_channel_names[];

  /**
    This lock was designed to protect the channel_map from adding or removing
    master_info objects from the map (adding or removing replication channels).
    In fact it also acts like the LOCK_active_mi of MySQL 5.6, preventing two
    replication administrative commands to run in parallel.
  */
  Checkable_rwlock *m_channel_map_lock;

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE

  /* Array for  replication performance schema related tables */
  Master_info *rpl_pfs_mi[MAX_CHANNELS];

#endif  /* WITH_PERFSCHEMA_STORAGE_ENGINE */

  /*
    A empty mi_map to allow Multisource_info::end() to return a
    valid constant value.
  */
  mi_map empty_mi_map;

public:

  /* Constructor for this class.*/
  Multisource_info()
  {
    /*
      This class should be a singleton.
      The assert below is to prevent it to be instantiated more than once.
    */
#ifndef NDEBUG
    static int instance_count= 0;
    instance_count++;
    assert(instance_count == 1);
#endif
    current_mi_count= 0;
    default_channel_mi= NULL;
#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE
    init_rpl_pfs_mi();
#endif  /* WITH_PERFSCHEMA_STORAGE_ENGINE */

    m_channel_map_lock= new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
                                             key_rwlock_channel_map_lock
#endif
                                            );
  }

  /* Destructor for this class.*/
  ~Multisource_info()
  {
    delete m_channel_map_lock;
  }

  /**
    Adds the Master_info object to both replication_channel_map and rpl_pfs_mi

    @param[in]  channel_name      channel name
    @param[in]  mi                pointer to master info corresponding
                                  to this channel
    @return
      @retval      false       succesfully added
      @retval      true        couldn't add channel
  */
  bool add_mi(const char* channel_name, Master_info* mi);

  /**
    Find the master_info object corresponding to a channel explicitly
    from replication channel_map;
    Return if it exists, otherwise return 0

    @param[in]  channel       channel name for the master info object.

    @retval                   pointer to the master info object if exists
                              in the map. Otherwise, NULL;
  */
  Master_info* get_mi(const char* channel_name);

  /**
    Return the master_info object corresponding to the default channel.
    @retval                   pointer to the master info object if exists.
                              Otherwise, NULL;
  */
  Master_info* get_default_channel_mi()
  {
    m_channel_map_lock->assert_some_lock();
    return default_channel_mi;
  }

  /**
    Remove the entry corresponding to the channel, from the
    replication_channel_map and sets index in the  multisource_mi to 0;
    And also delete the {mi, rli} pair corresponding to this channel

    @note this requires the caller to hold the mi->channel_wrlock.
    If the method succeeds the master info object is deleted and the lock
    is released. If the an error occurs and the method return true, the {mi}
    object wont be deleted and the caller should release the channel_wrlock.

    @param[in]    channel_name     Name of the channel for a Master_info
                                   object which must exist.

    @return true if an error occurred, false otherwise
  */
  bool delete_mi(const char* channel_name);

  /**
    Get the default channel for this multisourced_slave;
  */
  inline const char* get_default_channel()
  {
    return default_channel;
  }

  /**
    Get the number of instances of Master_info in the map.

    @param all  If it should count all channels.
                If false, only slave channels are counted.

    @return The number of channels or 0 if empty.
  */
  inline uint get_num_instances(bool all=false)
  {
    DBUG_ENTER("Multisource_info::get_num_instances");

    m_channel_map_lock->assert_some_lock();

    replication_channel_map::iterator map_it;

    if (all)
    {
      int count = 0;

      for (map_it= rep_channel_map.begin();
           map_it != rep_channel_map.end(); map_it++)
      {
        count += map_it->second.size();
      }
      DBUG_RETURN(count);
    }
    else //Return only the slave channels
    {
      map_it= rep_channel_map.find(SLAVE_REPLICATION_CHANNEL);

      if (map_it == rep_channel_map.end())
        DBUG_RETURN(0);
      else
        DBUG_RETURN(map_it->second.size());
    }
  }

  /**
    Get max channels allowed for this map.
  */
  inline uint get_max_channels()
  {
    return MAX_CHANNELS;
  }

  /**
    Returns true if the current number of channels in this slave
    is less than the MAX_CHANNLES
  */
  inline bool is_valid_channel_count()
  {
    m_channel_map_lock->assert_some_lock();
    bool is_valid= current_mi_count < MAX_CHANNELS;
    DBUG_EXECUTE_IF("max_replication_channels_exceeded",
                    is_valid= false;);
    return (is_valid);
  }

  /**
    Returns if a channel name is one of the reserved group replication names

    @param channel    the channel name to test
    @param is_applier compare only with applier name

    @return
      @retval      true   the name is a reserved name
      @retval      false  non reserved name
  */
  bool is_group_replication_channel_name(const char* channel,
                                         bool is_applier= false);

  /**
     Forward iterators to initiate traversing of a map.

     @todo: Not to expose iterators. But instead to return
            only Master_infos or create generators when
            c++11 is introduced.
  */
  mi_map::iterator begin(enum_channel_type channel_type=
                             SLAVE_REPLICATION_CHANNEL)
  {
    replication_channel_map::iterator map_it;
    map_it= rep_channel_map.find(channel_type);

    if (map_it != rep_channel_map.end())
    {
      return map_it->second.begin();
    }

    return end(channel_type);
  }

  mi_map::iterator end(enum_channel_type channel_type=
                           SLAVE_REPLICATION_CHANNEL)
  {
    replication_channel_map::iterator map_it;
    map_it= rep_channel_map.find(channel_type);

    if (map_it != rep_channel_map.end())
    {
      return map_it->second.end();
    }

    return empty_mi_map.end();
  }

private:

#ifdef WITH_PERFSCHEMA_STORAGE_ENGINE

  /* Initialize the rpl_pfs_mi array to NULLs */
  inline void init_rpl_pfs_mi()
  {
    for (uint i= 0; i< MAX_CHANNELS; i++)
      rpl_pfs_mi[i]= 0;
  }

  /**
     Add a master info pointer to the rpl_pfs_mi array at the first
     NULL;

     @param[in]        mi        master info object to be added.

     @return                     false if success.Else true.
  */
  bool add_mi_to_rpl_pfs_mi(Master_info *mi);

  /**
     Get the index of the master info correposponding to channel name
     from the rpl_pfs_mi array.
     @param[in]       channe_name     Channel name to get the index from

     @return         index of mi for the channel_name. Else -1;
  */
  int get_index_from_rpl_pfs_mi(const char* channel_name);

public:

  /**
    Used only by replication performance schema indices to get the master_info
    at the position 'pos' from the rpl_pfs_mi array.

    @param[in]   pos   the index in the rpl_pfs_mi array

    @retval            pointer to the master info object at pos 'pos';
  */
  Master_info* get_mi_at_pos(uint pos);
#endif /*WITH_PERFSCHEMA_STORAGE_ENGINE */

  /**
    Acquire the read lock.
  */
  inline void rdlock()
  { m_channel_map_lock->rdlock(); }

  /**
    Acquire the write lock.
  */
  inline void wrlock()
  { m_channel_map_lock->wrlock(); }

  /**
    Release the lock (whether it is a write or read lock).
  */
  inline void unlock()
  { m_channel_map_lock->unlock(); }

  /**
    Assert that some thread holds either the read or the write lock.
  */
  inline void assert_some_lock() const
  { m_channel_map_lock->assert_some_lock(); }

  /**
    Assert that some thread holds the write lock.
  */
  inline void assert_some_wrlock() const
  { m_channel_map_lock->assert_some_wrlock(); }
};

/* Global object for multisourced slave. */
extern Multisource_info channel_map;

static bool inline is_slave_configured()
{
  /* Server was started with server_id == 0
     OR
     failure to load slave info repositories because of repository
     mismatch i.e Assume slave had a multisource replication with several
     channels setup with TABLE repository. Then if the slave is restarted
     with FILE repository, we fail to load any of the slave repositories,
     including the default channel one.
     Hence, channel_map.get_default_channel_mi() will return NULL.
  */
  return (channel_map.get_default_channel_mi() != NULL);
}

#endif   /* HAVE_REPLICATION */
#endif  /*RPL_MSR_H*/
