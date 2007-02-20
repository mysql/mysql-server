/* Copyright (C) 2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

class WindowsService
{
protected:
  bool                  inited;
  const char            *serviceName;
  const char            *displayName;
  SERVICE_STATUS_HANDLE statusHandle;
  DWORD                 statusCheckpoint;
  SERVICE_STATUS        status;
  DWORD                 dwAcceptedControls;
  bool                  debugging;

public:
  WindowsService(const char *p_serviceName, const char *p_displayName);
  ~WindowsService(void);

  BOOL  Install(const char *username, const char *password);
  BOOL  Remove();
  BOOL  Init();
  BOOL  IsInstalled();
  void  SetAcceptedControls(DWORD acceptedControls);
  void  Debug(bool debugFlag) { debugging= debugFlag; }

public:
  static void WINAPI    ServiceMain(DWORD argc, LPTSTR *argv);
  static void WINAPI    ControlHandler(DWORD CtrlType);

protected:
  virtual void Run(DWORD argc, LPTSTR *argv)= 0;
  virtual void Stop()                 {}
  virtual void Shutdown()             {}
  virtual void Pause()                {}
  virtual void Continue()             {}
  virtual void Log(const char *msg)   {}

  BOOL ReportStatus(DWORD currentStatus, DWORD waitHint= 3000, DWORD dwError=0);
  void HandleControlCode(DWORD opcode);
  void RegisterAndRun(DWORD argc, LPTSTR *argv);
};
