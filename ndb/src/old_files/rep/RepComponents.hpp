/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef REPCOMPONENTS_HPP
#define REPCOMPONENTS_HPP

#include <rep/adapters/ExtNDB.hpp>
#include <rep/adapters/AppNDB.hpp>
#include <rep/transfer/TransPS.hpp>
#include <rep/transfer/TransSS.hpp>
#include <rep/Requestor.hpp>
#include <rep/state/RepState.hpp>

#include <rep/rep_version.hpp>

/**
 * Connection data
 */
class RepComponents {
public:
  RepComponents();
  ~RepComponents();

  int connectPS();
  int connectSS();

  ExtNDB *          m_extNDB;
  ExtAPI *          m_extAPI;
  TransPS *         m_transPS;
  
  TransSS *         m_transSS;
  AppNDB *          m_appNDB;

  Requestor *       m_requestor;

  GCIContainer *    m_gciContainer;
  GCIContainerPS *  m_gciContainerPS;

  char *            m_connectStringPS;
  char *            m_connectStringSS;

  RepState *        getRepState() { return m_repState; }
private:
  RepState *        m_repState;
};

#endif 
