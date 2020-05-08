@ECHO OFF
REM The following command updates the local intranet, trusted sites, and
REM internet zones to disable cross-domain requests. By default, SauceLabs
REM enables the option for these zones.
reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings\Zones\1" /v 1406 /t REG_DWORD /d 3 /f
reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings\Zones\2" /v 1406 /t REG_DWORD /d 3 /f
reg add "HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings\Zones\3" /v 1406 /t REG_DWORD /d 3 /f
