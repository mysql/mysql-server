/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdlib.h>
#include <string.h>
#include "gcs_message_stages.h"
#include <assert.h>
#include "gcs_internal_message.h"
#include "gcs_logging.h"
#include <mysql/gcs/xplatform/byteorder.h>
#include <sstream>
#include <iostream>
#include <map>

/*
  There will be a compile warning on os << type_code if no explicit type cast.
  the function is added to eliminate the warnings.
 */
template <class OSTREAM>
static inline
OSTREAM& operator << (OSTREAM& os, Gcs_message_stage::enum_type_code type_code)
{
  return os << static_cast<int>(type_code);
}


const unsigned short Gcs_message_stage::WIRE_HD_LEN_SIZE= 2;
const unsigned short Gcs_message_stage::WIRE_HD_TYPE_SIZE= 4;

const unsigned short Gcs_message_stage::WIRE_HD_LEN_OFFSET= 0;
const unsigned short Gcs_message_stage::WIRE_HD_TYPE_OFFSET=
  Gcs_message_stage::WIRE_HD_LEN_SIZE;

bool
Gcs_message_pipeline::outgoing(Gcs_packet &p)
{
  bool error= false;
  std::vector<Gcs_message_stage::enum_type_code>::iterator it;
  std::map<Gcs_message_stage::enum_type_code, Gcs_message_stage *>::iterator mit;
  for (it= m_pipeline.begin(); !error && it != m_pipeline.end(); it++)
  {
    if ((mit= m_stage_reg.find(*it)) != m_stage_reg.end())
      error= (*mit).second->apply(p);
    else
    {
      MYSQL_GCS_LOG_ERROR("Unable to deliver outgoing message. "
        << "Request for an unknown/invalid message handler! ("
        << *it << ")");
      error= true;
    }
  }
  return error;
}

bool
Gcs_message_pipeline::incoming(Gcs_packet &p)
{
  bool error;
  Gcs_message_stage::enum_type_code stage_type_code;
  std::map<Gcs_message_stage::enum_type_code, Gcs_message_stage *>::iterator mit;

  for (error= false; p.get_dyn_headers_length() > 0 && !error; )
  {
    // decode just the type code
    unsigned int i_stage_type_code;
    memcpy(&i_stage_type_code,
           p.get_payload() + Gcs_message_stage::WIRE_HD_TYPE_OFFSET,
           Gcs_message_stage::WIRE_HD_TYPE_SIZE);
    i_stage_type_code= le32toh(i_stage_type_code);
    stage_type_code= (Gcs_message_stage::enum_type_code) i_stage_type_code;

    if ((mit= m_stage_reg.find(stage_type_code)) != m_stage_reg.end())
      // apply the stage, which removes the header as well
      error= mit->second->revert(p);
    else
    {
      MYSQL_GCS_LOG_ERROR("Unable to deliver incoming message. "
        << "Request for an unknown/invalid message handler! ("
        << stage_type_code << ")");
      error= true;
    }
  }

  return error;
}

void Gcs_message_pipeline::register_stage(Gcs_message_stage *s)
{
  std::map<Gcs_message_stage::enum_type_code, Gcs_message_stage *>::iterator it;
  it = m_stage_reg.find(s->type_code());
  if (it != m_stage_reg.end())
  {
    /*
      If there is a previous registered stage, it is removed first.
    */
    delete (*it).second;
    m_stage_reg.erase(it);
  }
  m_stage_reg[s->type_code()]= s;
}

Gcs_message_pipeline::~Gcs_message_pipeline()
{
  std::map<Gcs_message_stage::enum_type_code, Gcs_message_stage *>::iterator it;
  for (it=m_stage_reg.begin(); it != m_stage_reg.end(); it++)
  {
    Gcs_message_stage *s= (*it).second;
    delete s;
  }
  m_pipeline.clear();
}
