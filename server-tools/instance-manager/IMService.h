#pragma once
#include "windowsservice.h"

class IMService : public WindowsService
{
public:
  IMService(void);
  ~IMService(void);

protected:
  void Log(const char *msg);
  void Stop();
  void Run(DWORD argc, LPTSTR *argv);
};
