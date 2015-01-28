#ifndef FAKE_KEY_INCLUDED
#define FAKE_KEY_INCLUDED

/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>

#include "key.h"                              // KEY


/**
  A fake class to make it easy to set up a KEY object.
 
  Note that in this version the KEY object is only initialized with necessary
  information to do operations on rec_per_key.
*/

class Fake_KEY : public KEY
{
public:
  /**
    Initialize the KEY object.

    Only member variables needed for the rec_per_key interface are
    currently initialized.

    @param key_parts_arg number of key parts this index should have
    @param unique        unique or non-unique key
  */

  Fake_KEY(unsigned int key_parts_arg, bool unique)
  {
    DBUG_ASSERT(key_parts_arg > 0);

    flags= 0;
    if (unique)
      flags|= HA_NOSAME;
    actual_flags= flags;

    user_defined_key_parts= key_parts_arg;
    actual_key_parts= key_parts_arg;

    // Allocate memory for the two rec_per_key arrays
    m_rec_per_key= new ulong[actual_key_parts];
    m_rec_per_key_float= new rec_per_key_t[actual_key_parts];
    set_rec_per_key_array(m_rec_per_key, m_rec_per_key_float);

    // Initialize the rec_per_key arrays with default/unknown value
    for (uint kp= 0; kp < actual_key_parts; kp++)
    {
      rec_per_key[kp]= 0;
      set_records_per_key(kp, REC_PER_KEY_UNKNOWN);
    } 
  }

  ~Fake_KEY()
  {
    // free the memory for the two rec_per_key arrays
    delete [] m_rec_per_key;
    delete [] m_rec_per_key_float;
  }

private:
  // Storage for the two records per key arrays
  ulong *m_rec_per_key;
  rec_per_key_t *m_rec_per_key_float;
};

#endif /* FAKE_KEY_INCLUDED */
