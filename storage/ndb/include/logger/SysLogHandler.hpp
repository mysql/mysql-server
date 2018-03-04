/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SYSLOGHANDLER_H
#define SYSLOGHANDLER_H

#include "LogHandler.hpp"

/**
 * Logs messages to syslog. The default identity is 'NDB'. 
 * See 'man 3 syslog'. 
 *
 * It logs the following severity levels.
 * <pre>
 *
 *  LOG_ALERT           A condition  that  should  be  corrected
 *                      immediately,  such as a corrupted system
 *                      database.
 *
 *  LOG_CRIT            Critical conditions, such as hard device
 *                      errors.
 *
 *  LOG_ERR             Errors.
 *
 *  LOG_WARNING         Warning messages.
 *
 *  LOG_INFO            Informational messages.
 *
 *  LOG_DEBUG           Messages that contain  information  nor-
 *                      mally  of use only when debugging a pro-
 *                      gram.
 * </pre>
 *
 * @see LogHandler
 * @version #@ $Id: SysLogHandler.hpp,v 1.2 2003/09/01 10:15:53 innpeno Exp $
 */
class SysLogHandler : public LogHandler
{
public:
  /**
   * Default constructor.
   */
  SysLogHandler();
  
  /**
   * Create a new syslog handler with the specified identity.
   *
   * @param pIdentity a syslog identity.
   * @param facility syslog facility, defaults to LOG_USER
   */
  SysLogHandler(const char* pIdentity, int facility);

  /**
   * Destructor.
   */
  virtual ~SysLogHandler();
  
  virtual bool open();
  virtual bool close();

  virtual bool is_open();

  virtual bool setParam(const BaseString &param, const BaseString &value);
  bool setFacility(const BaseString &facility);

protected:	
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level);
  virtual void writeMessage(const char* pMsg);
  virtual void writeFooter();

private:
  /** Prohibit*/
  SysLogHandler(const SysLogHandler&);
  SysLogHandler operator = (const SysLogHandler&);
  bool operator == (const SysLogHandler&);

  int m_severity;
  const char* m_pCategory;

  /** Syslog identity for all log entries. */
  const char* m_pIdentity;
  int m_facility;
  bool m_open;
};

#endif
