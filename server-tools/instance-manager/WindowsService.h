#pragma once

class WindowsService
{
protected:
  bool                  inited;
  const char            *serviceName;
  const char            *displayName;
  const char            *username;
  const char            *password;
  SERVICE_STATUS_HANDLE statusHandle;
  DWORD                 statusCheckpoint;
  SERVICE_STATUS        status;
  DWORD                 dwAcceptedControls;
  bool                  debugging;

public:
  WindowsService(void);
  ~WindowsService(void);

  BOOL  Install();
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
