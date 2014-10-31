#ifndef FAKE_COSTMODEL_INCLUDED
#define FAKE_COSTMODEL_INCLUDED

/*
   Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "opt_costconstants.h"
#include "opt_costmodel.h"

/**
  This is is a "fake" cost model that can be used in unit tests that
  do not link with the server libraries.
*/

class Fake_Cost_model_server : public Cost_model_server
{
public:
  Fake_Cost_model_server()
  {
    // Create default values for server cost constants
    m_server_cost_constants= new Server_cost_constants();
#if !defined(DBUG_OFF)
    m_initialized= true;
#endif
  }

  ~Fake_Cost_model_server()
  {
    delete m_server_cost_constants;
    m_server_cost_constants= NULL;
  }
};


class Fake_Cost_model_table : public Cost_model_table
{
public:
  Fake_Cost_model_table()
  {
    // Create a fake cost model server object that will provide
    // cost constants for server operations
    m_cost_model_server= new Fake_Cost_model_server();

    // Allocate cost constants for operations on tables
    m_se_cost_constants= new SE_cost_constants();

#if !defined(DBUG_OFF)
    m_initialized= true;
#endif
  }

  ~Fake_Cost_model_table()
  {
    delete m_cost_model_server;
    m_cost_model_server= NULL;
    delete m_se_cost_constants;
    m_se_cost_constants= NULL;
  }
};

#endif /* FAKE_COSTMODEL_INCLUDED */
