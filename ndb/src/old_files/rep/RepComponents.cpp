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

#include "RepComponents.hpp"

RepComponents::RepComponents()
{
  /**
   * @todo  Fix proper reporting of errors
   */
  m_connectStringPS = NULL;
  m_connectStringSS = NULL;

  /**
   * Phase 1: Containers, RepState
   */
  m_gciContainer = new GCIContainer(MAX_NODE_GROUPS);
  if (!m_gciContainer) REPABORT("Could not allocate object");
  m_gciContainerPS = new GCIContainerPS(MAX_NODE_GROUPS);
  if (!m_gciContainerPS) REPABORT("Could not allocate object");
  m_repState = new RepState();
  if (!m_repState) REPABORT("Could not allocate object");

  /**
   * Phase 2: PS 
   */
  m_transPS = new TransPS(m_gciContainerPS);
  if (!m_transPS) REPABORT("Could not allocate object");
  

  m_extAPI = new ExtAPI();
  if (!m_extAPI) REPABORT("Could not allocate object");

  m_extNDB = new ExtNDB(m_gciContainerPS, m_extAPI);
  if (!m_extNDB) REPABORT("Could not allocate object");

  /**
   * Phase 3: SS
   */
  m_transSS = new TransSS(m_gciContainer, m_repState);
  if (!m_transSS) REPABORT("Could not allocate object");
  m_appNDB = new AppNDB(m_gciContainer, m_repState);
  if (!m_appNDB) REPABORT("Could not allocate object");

  /**
   * Phase 4: Requestor 
   */
  m_requestor = new Requestor(m_gciContainer, m_appNDB, m_repState);
  if (!m_requestor) REPABORT("Could not allocate object");

  /**
   * Phase 5
   */
  m_repState->init(m_transSS->getRepSender());
  m_repState->setApplier(m_appNDB);
  m_repState->setGCIContainer(m_gciContainer);

  m_requestor->setRepSender(m_transSS->getRepSender());

  m_extNDB->setRepSender(m_transPS->getRepSender());

  m_transPS->setGrepSender(m_extNDB->getGrepSender());
}

RepComponents::~RepComponents()
{
  if (m_requestor) delete m_requestor;

  if (m_appNDB) delete m_appNDB;
  if (m_extNDB) delete m_extNDB;
  if (m_extAPI) delete m_extAPI;
  
  if (m_repState) delete m_repState;

  if (m_transPS) delete m_transPS;
  if (m_transSS) delete m_transSS;

  if (m_gciContainer) delete m_gciContainer;
  if (m_gciContainerPS) delete m_gciContainerPS;
}

int 
RepComponents::connectPS() 
{
  /**
   * @todo Fix return values of this function
   */

  /**
   * Phase 1: TransporterFacade 1, Block number: 2  (PS)
   */
  if (!m_extNDB->init(m_connectStringPS)) return -1;
  
  /**
   * Phase 2: TransporterFacade 2, Block number: 2  (PS)
   */
  m_transPS->init(m_transSS->getTransporterFacade(), m_connectStringPS);

  return 0;
}

int 
RepComponents::connectSS() 
{
  /**
   * @todo Fix return values of this function
   */

  /**
   * Phase 1: TransporterFacade 1, Block number: 1  (SS)
   */
  m_appNDB->init(m_connectStringSS);
  
  /**
   * Phase 2: TransporterFacade 2, Block number: 1  (SS)
   */
  m_transSS->init(m_connectStringSS);

  /**
   * Phase 3: Has no TransporterFacade, just starts thread
   */
  m_requestor->init();

  return 0;
}
