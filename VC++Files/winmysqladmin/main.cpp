//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "initsetup.h"
#include "db.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
#include <shellapi.h>
#include <registry.hpp>
#include <winsvc.h>
#include <winsock.h>
#include <shlobj.h>
#include <IniFiles.hpp>
#include <dir.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque.h>
#include <vector.h>
#include <fstream.h>
#include <iostream.h>
#include <iterator.h>
#include <sstream.h>
#include "mysql.h"
#include <Printers.hpp>

TForm1 *Form1;
bool i_start, NT;
bool IsForce = false;
bool IsVariables = false;
bool IsProcess  = false ;
bool IsDatabases = false;
bool new_line = 0;
bool ya = true;
bool yy = true;
bool rinit = false;
AnsiString vpath;
AnsiString vip;
MYSQL_RES *res_1;
static unsigned long q = 0;
bool preport = false;
bool treport = false;
bool ereport = false;
AnsiString mainroot;
bool IsMySQLNode = false;
MYSQL *MySQL;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
  i_start = true;
  IsConnect = false;
 if (ParamCount() > 0){
   if (ParamStr(1) == "-h" || ParamStr(1) == "h" ) {
     ShowHelp();  Application->Terminate();  }
   else if (ParamStr(1) == "-w" || ParamStr(1) == "w") {
     i_start = false;  ContinueLoad();   }
  }
 else {
   ContinueLoad();  Hide(); GetServerOptions(); }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::DrawItem(TMessage& Msg)
{
   IconDrawItem((LPDRAWITEMSTRUCT)Msg.LParam);
   TForm::Dispatch(&Msg);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::MyNotify(TMessage& Msg)
{
 POINT MousePos;

 switch(Msg.LParam) {
  case WM_RBUTTONUP:
    if (GetCursorPos(&MousePos)){
       PopupMenu1->PopupComponent = Form1; SetForegroundWindow(Handle);
       PopupMenu1->Popup(MousePos.x, MousePos.y);}
    else  Show();
    break;
  case WM_LBUTTONUP:
    if (GetCursorPos(&MousePos)){
       PopupMenu1->PopupComponent = Form1; SetForegroundWindow(Handle);
       PopupMenu1->Popup(MousePos.x, MousePos.y); }

    ToggleState();
     break;
  default:
     break; }

 TForm::Dispatch(&Msg);
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TrayMessage(DWORD dwMessage)
{
 NOTIFYICONDATA tnd;
 PSTR pszTip;

 pszTip = TipText();

 tnd.cbSize          = sizeof(NOTIFYICONDATA);
 tnd.hWnd            = Handle;
 tnd.uID             = IDC_MYICON;
 tnd.uFlags          = NIF_MESSAGE | NIF_ICON | NIF_TIP;
 tnd.uCallbackMessage	= MYWM_NOTIFY;

 if (dwMessage == NIM_MODIFY){
   tnd.hIcon	= IconHandle();
   if (pszTip)lstrcpyn(tnd.szTip, pszTip, sizeof(tnd.szTip));
   else tnd.szTip[0] = '\0'; }
 else { tnd.hIcon = NULL; tnd.szTip[0] = '\0'; }

 return (Shell_NotifyIcon(dwMessage, &tnd));
}
//---------------------------------------------------------------------------
HANDLE __fastcall TForm1::IconHandle(void)
{

 if (!NT){
   if (MySQLSignal()){Image3->Visible = false; Image2->Visible = true;
          return (Image2->Picture->Icon->Handle); }
   else              {Image2->Visible = false; Image3->Visible = true;
          return (Image3->Picture->Icon->Handle); }
    }
 else   {
   if (TheServiceStatus()){Image3->Visible = false; Image2->Visible = true;
          return (Image2->Picture->Icon->Handle); }

   else if (MySQLSignal()){Image3->Visible = false; Image2->Visible = true;
          return (Image2->Picture->Icon->Handle); }
   else                   {Image2->Visible = false; Image3->Visible = true;
          return (Image3->Picture->Icon->Handle); }
    }

}
//---------------------------------------------------------------------------
void __fastcall TForm1::ToggleState(void)
{

 TrayMessage(NIM_MODIFY);
 if (!NT){
   if (MySQLSignal()){SSW9->Caption = "ShutDown the Server";
                     Image3->Visible = false; Image2->Visible = true; }
   else              {SSW9->Caption = "Start the Server";
                     Image2->Visible = false; Image3->Visible = true; }
         }
 else    {
   if (TheServiceStart()) {
      Standa->Enabled = false;
      if (TheServiceStatus()) {RService->Enabled = false;
                     StopS->Enabled = true;
                     StopS->Caption = "Stop the Service";
                     Image3->Visible = false;
                     Image2->Visible = true; }
      else                    {RService->Enabled = true;
                     StopS->Enabled = true;
                     RService->Caption = "Remove the Service";
                     StopS->Caption = "Start the Service";
                     Image2->Visible = false;
                     Image3->Visible = true; }
                          }
    else                  {
       Standa->Enabled = true;
       StopS->Enabled = false;
       if (MySQLSignal()) {
                     RService->Enabled = false;
                     Standa->Caption = "ShutDown the Server Standalone";
                     Image3->Visible = false;
                     Image2->Visible = true; }

       else               {
                     RService->Enabled = true;
                     RService->Caption = "Install the Service";
                     Standa->Caption = "Start the Server Standalone";
                     Image2->Visible = false;
                     Image3->Visible = true; }

                           }


          }

}
//---------------------------------------------------------------------------
PSTR __fastcall TForm1::TipText(void)
{
  char* status = StatusLine->SimpleText.c_str();
  return status;

}
//---------------------------------------------------------------------------
void __fastcall TForm1::WMQueryEndSession(TWMQueryEndSession &msg)
{


  if (!NT) {

    if (MySQLSignal()){
                        StatusLine->SimpleText = "Shutdown in progress.....";
                        Show(); Shutd();  msg.Result = 1; }
    else              {
                        StatusLine->SimpleText = "The Server already is down......";
                        Show();  msg.Result = 1;  Close(); }
           }
  else     {

    Show();
    if                 (!TheServiceStart())  { if (MySQLSignal()) Shutd();   }
                        msg.Result = 1;
           }

}

//---------------------------------------------------------------------------
LRESULT IconDrawItem(LPDRAWITEMSTRUCT lpdi)
{
   HICON hIcon;

   hIcon = (HICON)LoadImage(g_hinst, MAKEINTRESOURCE(lpdi->CtlID), IMAGE_ICON,
		16, 16, 0);
   if (!hIcon)
   	return(false);

   DrawIconEx(lpdi->hDC, lpdi->rcItem.left, lpdi->rcItem.top, hIcon,
		16, 16, 0, NULL, DI_NORMAL);

   return(true);
}
//---------------------------------------------------------------------------
AnsiString __fastcall TForm1::TheComputer()
{
  AnsiString theword;
  DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
  char szBuf[MAX_COMPUTERNAME_LENGTH + 1];
  szBuf[0] = '\0';

  GetComputerName(szBuf, &dwSize);
  theword = (AnsiString) szBuf;
  delete [] szBuf;
  return theword;

}
//---------------------------------------------------------------------------
AnsiString __fastcall TForm1::TheOS()
{
  AnsiString theword;
  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);

  switch (info.dwPlatformId)
    {
      case VER_PLATFORM_WIN32s:
        NT = false;
        theword = "Win32s detected";
        break;
      case VER_PLATFORM_WIN32_WINDOWS:
        NT = false;
        theword = "Win 95 or Win 98 detected";
        break;
      case VER_PLATFORM_WIN32_NT:
        NT = true;
        theword = "Windows NT detected";
        break;
     }
     return theword;
}
///---------------------------------------------------------------------------
AnsiString __fastcall TForm1::TheUser()
{
  AnsiString theword;
  DWORD dwSize = 0;

  GetUserName(NULL, &dwSize);

  char *szBuf = new char[dwSize];
  szBuf[0] = '\0';

  GetUserName(szBuf, &dwSize);
  theword = (AnsiString) szBuf;
  delete [] szBuf;
  return theword;

}
//---------------------------------------------------------------------------
void __fastcall TForm1::TakeIP(void)
{
  WORD wVersionRequested;
  WSADATA WSAData;
  wVersionRequested = MAKEWORD(1,1);
  WSAStartup(wVersionRequested,&WSAData);

  hostent *P;
  char s[128];
  in_addr in;
  char *P2;
  gethostname(s, 128);
  P = gethostbyname(s);

  Memo2->Lines->Clear();
  Memo2->Lines->Add((AnsiString)P->h_name);
  mainroot = P->h_name;
  in.S_un.S_un_b.s_b1 = P->h_addr_list[0][0];
  in.S_un.S_un_b.s_b2 = P->h_addr_list[0][1];
  in.S_un.S_un_b.s_b3 = P->h_addr_list[0][2];
  in.S_un.S_un_b.s_b4 = P->h_addr_list[0][3];
  P2 = inet_ntoa(in);
  vip = P2;
  mainroot += " ( " + (AnsiString)P2 + " )";
  Memo2->Lines->Add(P2);

}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetmemStatus(void)
{
  MEMORYSTATUS ms;
  ms.dwLength = sizeof(MEMORYSTATUS);
  GlobalMemoryStatus(&ms);

  Edit2->Text = AnsiString((double)ms.dwTotalPhys / 1024000.0) + " MB RAM";
}

//---------------------------------------------------------------------------
void __fastcall TForm1::ShowHelp(void)
{
   Application->MessageBox("Usage: WinMySQLadmin.EXE [OPTIONS]\n\n-w    Run the tool without start the Server.\n-h        Shows this message and exit ", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::ContinueLoad(void)
{
 OS->Text = TheOS();
 Localhost->Text = TheComputer();
 Localuser->Text = TheUser();
 GetmemStatus();
 ClearBox();
 TakeIP();
 MyODBC();


 IsMyIniUp();

 if (!NT) { WinNT->Enabled = false; NtVer->Enabled = false; Win9->Enabled  = true; }
 else     { WinNT->Enabled = true;   Win9->Enabled  = false;   }

 if (i_start)
   {
     // NT never is started from the prompt
     if ((!NT) && (!MySQLSignal())) mysqldstart();
      {
       TrayMessage(NIM_MODIFY);
       SeekErrFile();
      }
   }
  Hide();

}

//---------------------------------------------------------------------------
void __fastcall TForm1::MyODBC(void)
{

  TRegistry *Registry = new TRegistry();
  Memo3->Lines->Clear();

  try
   {
     Registry->RootKey = HKEY_LOCAL_MACHINE;
     // the basic data of myodbc
     if (Registry->OpenKey("Software\\ODBC\\ODBCINST.INI\\MySQL", false))
       {
         Memo3->Lines->Add("Driver Version\t" + Registry->ReadString("DriverODBCVer"));
         Memo3->Lines->Add("Driver\t\t" + Registry->ReadString("Driver"));
         Memo3->Lines->Add("API Level\t\t" + Registry->ReadString("APILevel"));
         Memo3->Lines->Add("Setup\t\t" + Registry->ReadString("Setup"));
         Memo3->Lines->Add("SQL Level\t" + Registry->ReadString("SQLLevel"));
       }
      else
         Memo3->Lines->Add("Not Found");

   }
  catch (...)
   {
    delete Registry;
   }
   Memo3->Enabled = false;
}
//---------------------------------------------------------------------------

void __fastcall TForm1::IsMyIniUp(void)
{
  // we see if the my.ini is Up
 AnsiString asFileName = FileSearch("my.ini", TheWinDir());
 if (asFileName.IsEmpty())
   {
     IsForce = true;
     i_start = false;
     QuickSearch();
   }
 else
   {
     Memo1->Enabled = true;
     Memo1->Lines->Clear();
     FillMyIni();
     GetBaseDir();
   }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::QuickSearch(void)
{
 AnsiString asFileName = FileSearch("mysql.exe", "c:/mysql/bin");
 if (!asFileName.IsEmpty())
  BaseDir->Text = "c:/mysql";
}
//---------------------------------------------------------------------------
AnsiString __fastcall TForm1::TheWinDir()
{
  AnsiString WinDir;
  UINT       BufferSize = GetWindowsDirectory(NULL,0);
  WinDir.SetLength(BufferSize+1);
  GetWindowsDirectory(WinDir.c_str(),BufferSize);
  char* dirw = WinDir.c_str();
  return dirw ;

}
//---------------------------------------------------------------------------
void __fastcall TForm1::FillMyIni(void)
{
 Memo1->Lines->LoadFromFile(TheWinDir() + "\\my.ini");

}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetBaseDir(void)
{

 char drive[_MAX_DRIVE];
 char dir[_MAX_DIR];
 char file[_MAX_FNAME];
 char ext[_MAX_EXT];


 TIniFile *pIniFile = new
 TIniFile(TheWinDir() + "\\my.ini");

 BaseDir->Text = pIniFile->ReadString("mysqld","basedir","")  ;
 AnsiString lx = pIniFile->ReadString("WinMySQLadmin","Server","")  ;
 _splitpath((lx).c_str(),drive,dir,file,ext);
 AnsiString lw = (AnsiString) file + ext;

 if ( lw == "mysqld-shareware.exe") {ShareVer->Checked = true;}
 if ( lw == "mysqld.exe") {MysqldVer->Checked = true;}
 if ( lw == "mysqld-opt.exe") {OptVer->Checked = true;}
 if ( lw == "mysqld-nt.exe") {NtVer->Checked = true;}

 delete pIniFile;

}
//---------------------------------------------------------------------------
void __fastcall TForm1::Showme1Click(TObject *Sender)
{
 if(Showme1->Caption == "Show me")  {  TrayMessage(NIM_DELETE);
       Showme1->Caption = "Hide me";  Show();  }
  else  { TrayMessage(NIM_ADD);  TrayMessage(NIM_MODIFY);
       Showme1->Caption = "Show me";  Hide();  }
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::MySQLSignal()
{
  HANDLE hEventShutdown;
  hEventShutdown=OpenEvent(EVENT_MODIFY_STATE, 0, "MySqlShutdown");

  if(hEventShutdown)
    {
      CloseHandle(hEventShutdown);
      return true;
    }
  else
    {
     CloseHandle(hEventShutdown);
     return false;
    }

}

//---------------------------------------------------------------------------
bool __fastcall TForm1::mysqldstart()
{
    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow |= SW_SHOWNORMAL;


    TIniFile *pIniFile = new
    TIniFile(TheWinDir() + "\\my.ini");

    if (NT)
     vpath =   pIniFile->ReadString("WinMySQLadmin","Server","") + " --standalone\0" ;
    else
      vpath =   pIniFile->ReadString("WinMySQLadmin","Server","") + "\0" ;

    if ( ! CreateProcess(0,vpath.c_str(), 0, 0, 0, 0, 0, 0, &si,&pi))
     {
       TrayMessage(NIM_MODIFY);
       return false;
     }
    else
      {
        TrayMessage(NIM_MODIFY);
        return true;

      }

}

//---------------------------------------------------------------------------
bool __fastcall TForm1::SeekErrFile()
{
  Memo4->Enabled = true;
  Memo4->Lines->Clear();
  AnsiString asFileName = FileSearch("mysql.err", BaseDir->Text + "/data");
  if (!asFileName.IsEmpty())
   {
     FName = BaseDir->Text + "/data/mysql.err";
     ifstream in((FName).c_str());
     in.seekg(0, ios::end);
     string s, line;
     deque<string> v;
     deque<string> lines;
     streampos sp = in.tellg();
     if (sp <= 1000)
      in.seekg(0, ios::beg);
    else
     {
      in.seekg(0, ios::beg);
      in.seekg((sp - 1000));
     }

     do {
          lines.push_back(line);
     }while (getline(in, line));


     if( lines.size() <= 15)
      {
        deque<string>::reverse_iterator r;
        for(r = lines.rbegin(); r != lines.rend() ; r++)
         {
          if (ereport)
           Memo5->Lines->Add((*r).c_str());
           Memo4->Lines->Add((*r).c_str());

         }
      }
     else
      {
        int k = 0;
        deque<string>::reverse_iterator r;
        for(r = lines.rbegin(); r != lines.rend(); r++)
         {
           if (ereport)
           Memo5->Lines->Add((*r).c_str());
           Memo4->Lines->Add((*r).c_str());
           if (++k >= 15) { break;}
         }
      }
      in.close();
      return true;
   }
  else
    return false;

}

//---------------------------------------------------------------------------
void __fastcall TForm1::Timer1Timer(TObject *Sender)
{
 Showme1->Caption = "Show me";
  TrayMessage(NIM_ADD);
  TrayMessage(NIM_MODIFY);
  Hide();
  if (IsForce) {Form2->Show();}
  Timer1->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetServerFile()
{

  AnsiString FileName;

 if(!NT) {
   FileName =  FileSearch("mysqld-opt.exe", ExtractFilePath(Application->ExeName));
   if (FileName.IsEmpty())  FileName =  FileSearch("mysqld.exe", ExtractFilePath(Application->ExeName));
   if (FileName.IsEmpty())    FileName =  FileSearch("mysqld-shareware.exe", ExtractFilePath(Application->ExeName));

   if (!FileName.IsEmpty()){
      if ( FileName == "mysqld-opt.exe") {OptVer->Checked = true;}
      if ( FileName == "mysqld.exe") {MysqldVer->Checked= true;}
      if ( FileName == "mysqld-shareware.exe") {ShareVer->Checked= true;} }

  }
 else {

   FileName =  FileSearch("mysqld-nt.exe", ExtractFilePath(Application->ExeName));
   if (FileName.IsEmpty())  FileName =  FileSearch("mysqld.exe", ExtractFilePath(Application->ExeName));
   if (FileName.IsEmpty())  FileName =  FileSearch("mysqld-shareware.exe", ExtractFilePath(Application->ExeName));

   if (!FileName.IsEmpty()) {
      if ( FileName == "mysqld-nt.exe") {NtVer->Checked = true;}
      if ( FileName == "mysqld.exe") {MysqldVer->Checked= true;}
      if ( FileName == "mysqld-shareware.exe") {ShareVer->Checked= true;} }

     }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::CreateMyIniFile(void)
{
  char szFileName[6];
  int iFileHandle;
  AnsiString jk;

  Memo1->Enabled = true;
  Memo1->Lines->Clear();
  strcpy(szFileName,"\\my.ini");
  iFileHandle = FileCreate(TheWinDir() + szFileName );

  jk = "#This File was made using the WinMySQLadmin 1.0 Tool\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#" + Now() + "\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#Uncomment or Add only the keys that you know how works.\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#Read the MySQL Manual for instructions\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());


  jk = "[mysqld]\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

   jk = "basedir=" + TheDir() + "\n";
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#bind-address=" + vip + "\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#datadir=" + TheDir() + "/data\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#language=" + TheDir() + "/share/your language directory\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#delay-key-write-for-all-tables\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#log-long-format\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#slow query log=#\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#tmpdir=#\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#ansi\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#new\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#port=3306\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#safe\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#skip-name-resolve\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#skip-networking\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#skip-new\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#skip-host-cache\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#set-variable = key_buffer=16M\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#set-variable = max_allowed_packet=1M\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#set-variable = thread_stack=128K\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

    jk = "#set-variable = flush_time=1800\n\n" ;
    FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "[mysqldump]\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#quick\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#set-variable = max_allowed_packet=16M\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "[mysql]\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#no-auto-rehash\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "[isamchk]\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "#set-variable= key=16M\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "[WinMySQLadmin]\n\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());


  if (ShareVer->Checked) { jk = "Server=" + TheDir() + "/bin/mysqld-shareware.exe\n\n";}
  if (MysqldVer->Checked) {jk = "Server=" + TheDir() + "/bin/mysqld.exe\n\n";}
  if (OptVer->Checked) {jk = "Server=" + TheDir() + "/bin/mysqld-opt.exe\n\n";}
  if (NtVer->Checked) {jk = "Server=" + TheDir() + "/bin/mysqld-nt.exe\n\n";}
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "user=" + Form2->Edit1->Text + "\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  jk = "password=" + Form2->Edit2->Text + "\n" ;
  FileWrite(iFileHandle, (jk).c_str(), (jk).Length());

  FileClose(iFileHandle);
  FillMyIni();

}

//---------------------------------------------------------------------------
bool __fastcall TForm1::CreatingShortCut()
{
  // Where is The Start Menu in this Machine ?
  LPITEMIDLIST  pidl;
  LPMALLOC      pShellMalloc;
  char          szDir[MAX_PATH + 16];
  AnsiString file;
  AnsiString jk = "\\WinMySQLadmin.lnk" ;

  if(SUCCEEDED(SHGetMalloc(&pShellMalloc)))
   {
     if(SUCCEEDED(SHGetSpecialFolderLocation(NULL,
          CSIDL_STARTUP, &pidl)))
      {
        if(!SHGetPathFromIDList(pidl, szDir))
          {
            pShellMalloc->Release();
            pShellMalloc->Free(pidl);
            return false;
           }

          pShellMalloc->Free(pidl);
       }

        pShellMalloc->Release();
        StrCat(szDir, jk.c_str());
    }

 // the create

 IShellLink* pLink;
 IPersistFile* pPersistFile;

 if(SUCCEEDED(CoInitialize(NULL)))
   {
     if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL,
                     CLSCTX_INPROC_SERVER,
                     IID_IShellLink, (void **) &pLink)))
       {

         pLink->SetPath((ExtractFilePath(Application->ExeName) + "WinMySQLadmin.exe").c_str());
         pLink->SetDescription("WinMySQLadmin Tool");
         pLink->SetShowCmd(SW_SHOW);

            if(SUCCEEDED(pLink->QueryInterface(IID_IPersistFile,
                                               (void **)&pPersistFile)))
            {

                WideString strShortCutLocation(szDir);
                pPersistFile->Save(strShortCutLocation.c_bstr(), TRUE);
                pPersistFile->Release();
            }
               pLink->Release();
        }

        CoUninitialize();
    }


  return true;
}

//---------------------------------------------------------------------------
AnsiString __fastcall TForm1::TheDir()
{
 AnsiString buffer;
 char s[_MAX_PATH + 1];

 StrCopy(s, ( BaseDir->Text).c_str()) ;

 for (int i = 0; s[i] != NULL; i++)
  if (s[i] != '\\')
    buffer += s[i];
  else
    buffer += "/";

 return buffer;

}

//---------------------------------------------------------------------------
void __fastcall TForm1::SpeedButton1Click(TObject *Sender)
{
 Application->HelpCommand(HELP_FINDER,0);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Timer2Timer(TObject *Sender)
{
 ToggleState();

}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServiceStart()
{
  bool thatok;
  char *SERVICE_NAME = "MySql";
  SC_HANDLE myService, scm;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS | GENERIC_WRITE);
  if (scm)
   {
     myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
     if (myService)
      thatok = true;
     else
      thatok = false;
   }
   CloseServiceHandle(myService);
   CloseServiceHandle(scm);
   return thatok;
}

//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServicePause()
{

  bool thatok;
  char *SERVICE_NAME = "MySql";
  SC_HANDLE myService, scm;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);

  if (scm)
    {
       myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
       if (myService)
         {
           // stop the service
           if (IsConnect)
            {
               mysql_kill(MySQL,mysql_thread_id(MySQL));
               StatusLine->SimpleText = "";
               q = 0;
            }


            SERVICE_STATUS ss;
            thatok = ControlService(myService,
                            SERVICE_CONTROL_STOP,
                            &ss);

         }
       else
       thatok = false;
    }
   else
      thatok = false;

   CloseServiceHandle(myService);
   CloseServiceHandle(scm);
   return thatok;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServiceResume()
{

  bool thatok;
  char *SERVICE_NAME = "MySql";
  SC_HANDLE myService, scm;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);

  if (scm)
    {
       myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
       if (myService)
         {
           // start the service

            thatok = StartService(myService, 0, NULL);
         }
       else
        thatok = false;
    }
   else
      thatok = false;

   CloseServiceHandle(myService);
   CloseServiceHandle(scm);
   return thatok;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServiceStatus()
{
  bool thatok;
  bool k;
  char *SERVICE_NAME = "MySql";
  SC_HANDLE myService, scm;
  SERVICE_STATUS ss;
  DWORD dwState = 0xFFFFFFFF;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);

  if (scm)
    {
       myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
       if (myService)
         {
           memset(&ss, 0, sizeof(ss));
           k = QueryServiceStatus(myService,&ss);
           if (k)
             {
               dwState = ss.dwCurrentState;
               if (dwState == SERVICE_RUNNING)
                 thatok = true;
             }
           else
            thatok = false;
         }
       else
        thatok = false;
    }
   else
      thatok = false;

   CloseServiceHandle(myService);
   CloseServiceHandle(scm);
   return thatok;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServiceCreate()

{
  bool thatok;
  char *SERVICE_NAME = "MySql";
  char *szFullPath = vpath.c_str();
  SC_HANDLE myService, scm;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);

  if (scm)
   {  myService = CreateService(
	scm,
	SERVICE_NAME,
	SERVICE_NAME,
	SERVICE_ALL_ACCESS,
	SERVICE_WIN32_OWN_PROCESS,
	SERVICE_AUTO_START	,
	SERVICE_ERROR_NORMAL,
	szFullPath,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL);

     if (myService)
       thatok = true;
     else
       thatok = false;

  }

   CloseServiceHandle(myService);
   CloseServiceHandle(scm);
   return thatok;

}
//---------------------------------------------------------------------------

void __fastcall TForm1::Swin9Click(TObject *Sender)
{
 if(Application->MessageBox("Shutdown this tool", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
  Close();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SSW9Click(TObject *Sender)
{
 if (MySQLSignal())
    {
      if(Application->MessageBox("Shutdown the MySQL Server ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
        {

          if (Shutd())
            {
              IsConnect = false;
              IsVariables = false;
              IsProcess = false;
              IsDatabases = false;
              ya = false;
              ClearBox();
              Sleep(500);
              TrayMessage(NIM_MODIFY);

            }
          else
              Application->MessageBox("Fails to Shutdown the Server", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
        }
    }
   else
     {
       if(Application->MessageBox("Start the MySQL Server ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
        {
          if (mysqldstart())
            {
              TrayMessage(NIM_MODIFY);
              ya = true;
            }
           else
             Application->MessageBox("Fails to Start the Server", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);

        }
     }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ShutDownBoth1Click(TObject *Sender)
{
  if (MySQLSignal())
  {
    if(Application->MessageBox("Shutdown the MySQL Server and this tool ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
     {

       if (Shutd())
        Close();
       else
         {
           Application->MessageBox("Fails to Shutdown the Server", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);

         }
     }
   }
 else
    if(Application->MessageBox("Shutdown this tool ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
    Close();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ShutDownthisTool1Click(TObject *Sender)
{
 if(Application->MessageBox("Shutdown this tool ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
   Close();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StopSClick(TObject *Sender)
{
  AnsiString theWarning;
  theWarning = "Are you sure to stop the Service ?\n\nAll the connections will be loss !" ;
  if (TheServiceStatus())
    {
      if(Application->MessageBox(theWarning.c_str(), "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
        {
          if (TheServicePause())
            {
              TrayMessage(NIM_MODIFY);
              IsConnect = false;
              IsVariables = false;
              IsProcess = false;
              IsDatabases = false;
              ya = false;
              ClearBox();

            }
          else
             Application->MessageBox("Fails to stop the Service", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);

        }
    }
   else
     {
       if(Application->MessageBox("Start the Service Manager for the MySQL Server ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
         {
           if (TheServiceResume())
             {
               ya = true;
               TrayMessage(NIM_MODIFY);
             }
           else
               Application->MessageBox("Fails to start the Service", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
         }
     }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::RServiceClick(TObject *Sender)
{
 if (TheServiceStart())
    {
      if(Application->MessageBox("Remove the MySQL Server service ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
       {
         if (!TheServiceRemove())
         Application->MessageBox("Fails to Remove The MySQL Server Service", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
       }
    }
  else
    {
      if(Application->MessageBox("Install the MySQL Server service ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
       {
         if (!TheServerPath())
           Application->MessageBox("Please create first the my.ini setup", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
         else
           {
             if (!TheServiceCreate())
              Application->MessageBox("Fails to Install The MySQL Server Service", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
           }

       }
    }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::StandaClick(TObject *Sender)
{
  if (MySQLSignal())
    {
      if(Application->MessageBox("Shutdown the MySQL Server ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
        {
          if (Shutd())
           {
             IsConnect = false;
             IsVariables = false;
             IsProcess = false;
             IsDatabases = false;
             ya = false;
             ClearBox();
             Sleep(500);
             TrayMessage(NIM_MODIFY);

            }
          else
            Application->MessageBox("Fails to Shutdown the Server", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
        }
    }
   else
     {
       if(Application->MessageBox("Start the MySQL Server ", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
        {
          if (mysqldstart())
            {
              StatusLine->SimpleText = "";
              TrayMessage(NIM_MODIFY);

            }
          else
          Application->MessageBox("Fails to Start the Server", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
        }
     }
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::Shutd()
{
  // from Irena
  HANDLE hEventShutdown;
  hEventShutdown=OpenEvent(EVENT_MODIFY_STATE, 0, "MySqlShutdown");

  if (IsConnect)
   {
     mysql_kill(MySQL,mysql_thread_id(MySQL));
     mysql_shutdown(MySQL, SHUTDOWN_DEFAULT);
     StatusLine->SimpleText = "";

   }

   q = 0;


  if(hEventShutdown)
   {
    SetEvent(hEventShutdown);
    CloseHandle(hEventShutdown);
    TrayMessage(NIM_MODIFY);
    IsConnect = false;
    return true;
   }
  else
   {
     TrayMessage(NIM_MODIFY);
     return false;
   }

}
//---------------------------------------------------------------------------
void __fastcall TForm1::ClearBox(void)
{
 
 st22->Text = "";
 st23->Text = "";
 st24->Text = "";
 st25->Text = "";
 st26->Text = "";
 st27->Text = "";
 st28->Text = "";
 st29->Text = "";
 Edit3->Text = "";
 Edit4->Text = "";
 Edit5->Text = "";
 Edit6->Text = "";

}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServiceRemove()
{
  bool thatok;
  char *SERVICE_NAME = "MySql";
  SC_HANDLE myService, scm;
  scm = OpenSCManager(0, 0, SC_MANAGER_ALL_ACCESS);
  if (scm)
    {
      myService = OpenService(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
       if (myService)
         {
           if(DeleteService(myService))
             {
               CloseServiceHandle(myService);
               CloseServiceHandle(scm);
               thatok = true;
             }
           else
             {
                CloseServiceHandle(myService);
                CloseServiceHandle(scm);
                thatok = false;
              }

          }
       else
          {
            CloseServiceHandle(myService);
            CloseServiceHandle(scm);
            thatok = false;
          }
    }
  else
     {
      thatok = false;
      CloseServiceHandle(scm);
     }

   return thatok;

}
//---------------------------------------------------------------------------
bool __fastcall TForm1::TheServerPath()
{

 TIniFile *pIniFile = new
 TIniFile(TheWinDir() + "\\my.ini");

 vpath = pIniFile->ReadString("WinMySQLadmin","Server","")  ;
 delete pIniFile;
 if (vpath.IsEmpty())
   return false;
 else
   return true;

}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button5Click(TObject *Sender)
{
 if (!SeekErrFile())
  Application->MessageBox("Fails to find mysql.err", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::IsMySQLInit(void)
{
  AnsiString theCommand;
  char	*host = NULL,*password=0,*user=0 ;
  TIniFile *pIniFile = new
  TIniFile(TheWinDir() + "\\my.ini");

  AnsiString MyUser = pIniFile->ReadString("WinMySQLadmin","user","")  ;
  AnsiString MyPass = pIniFile->ReadString("WinMySQLadmin","password","")  ;

  delete pIniFile;


 if (!MyUser.IsEmpty() && MyUser.Length() && !MyPass.IsEmpty() && MyPass.Length())
   {
     if (!IsConnect)
      {

       MySQL = mysql_init(MySQL);
       if (mysql_real_connect(MySQL, "localhost",(MyUser).c_str(), (MyPass).c_str() , 0, 0, NULL, 0))
        IsConnect = true;
       else
        {
         if(mysql_real_connect(MySQL,host,user,password , 0, 0, NULL, 0))
          {
            IsConnect = true;
            theCommand = "GRANT ALL PRIVILEGES ON *.* TO ";
            theCommand +=  "'" + MyUser + "' @localhost IDENTIFIED BY ";
            theCommand +=  "'" + MyPass + "' with GRANT OPTION";
            char* los = theCommand.c_str();
            if(!mysql_query(MySQL, los ))
             StatusLine->SimpleText = " ";
          }

        }
       MySQL->reconnect= 1;

      }

  }
 else
  {
   if (!IsConnect)
    {
      MySQL = mysql_init(MySQL);
      if(mysql_real_connect(MySQL,host,user,password , 0, 0, NULL, 0))
      IsConnect = true;
      MySQL->reconnect= 1;
    }
  }
}

//---------------------------------------------------------------------------

void __fastcall TForm1::Timer3Timer(TObject *Sender)
{
   if ((NT) && TheServiceStatus()) {IsMySQLInit(); }

  if ((NT) && !TheServiceStatus() && MySQLSignal()) {IsMySQLInit(); }

  if (!(NT) && MySQLSignal()) {IsMySQLInit(); }

  if (IsConnect)
   {
     GetServerStatus();
     if (!IsMySQLNode)
     GetMainRoot();
     Extended->Enabled = true;
     if (!IsProcess && !GetProcess())
      StatusLine->SimpleText = "";
     if (!IsVariables && !GetVariables())
        StatusLine->SimpleText = "";
     Timer3->Interval = 10000;
   }
  else
    Extended->Enabled = false;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetServerStatus(void)
{

  GetExtendedStatus();
  Edit3->Text = mysql_get_server_info(MySQL);
  Edit4->Text = mysql_get_host_info(MySQL);
  Edit5->Text = mysql_get_client_info();
  Edit6->Text = mysql_get_proto_info(MySQL);


}

//---------------------------------------------------------------------------
bool __fastcall TForm1::GetProcess()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  unsigned int i;
  int k = 0;
  int therow = 1;
  new_line=1;

   StringGrid2->RowCount= 2;

   if (!(res=mysql_list_processes(MySQL)))
      {
       	return false;
      }

      while ((row=mysql_fetch_row(res)) != 0)
       {
         mysql_field_seek(res,0);
         StringGrid2->Cells[0][0] = "PID";
         StringGrid2->Cells[1][0] = "User";
         StringGrid2->Cells[2][0] = "Host";
         StringGrid2->Cells[3][0] = "DB";
         StringGrid2->Cells[4][0] = "Command";
         StringGrid2->Cells[5][0] = "Time";
         StringGrid2->Cells[6][0] = "State";
         StringGrid2->Cells[7][0] = "Info";
         for (i=0 ; i < mysql_num_fields(res); i++)
          {

             if (k <= 6 )
              {
               StringGrid2->Cells[k][therow] = row[i];
               k++;
              }
             else
               {

                 StringGrid2->Cells[(k)][therow] = row[i];
                 k = 0;
                 therow++ ;
                 StringGrid2->RowCount++;

               }

          }

       }

      StringGrid2->RowCount--;
      mysql_free_result(res);
      StringGrid5->RowCount--;
      IsProcess = true;
      return true;

}
//---------------------------------------------------------------------------
bool __fastcall TForm1::GetVariables()
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  unsigned int i;
  int k = 1;
  new_line=1;
  bool left = true;
  AnsiString report;
  StringGrid1->RowCount = 2;
   if (mysql_query(MySQL,"show variables") ||
	  !(res=mysql_store_result(MySQL)))
      {
        return false;
      }

      while ((row=mysql_fetch_row(res)) != 0)
       {
         mysql_field_seek(res,0);

         StringGrid1->Cells[0][0] = "Variable Name";
         StringGrid1->Cells[1][0] = "Value";


         for (i=0 ; i < mysql_num_fields(res); i++)
          {

             if (left)
              {
                 if (treport)
                 report = GetString(row[i]);
                 StringGrid1->Cells[0][k++] = row[i];
                 left = false;
              }
             else
               {
                 if (treport)
                  Memo5->Lines->Add(report + row[i]);
                 StringGrid1->RowCount++;
                 StringGrid1->Cells[1][--k] = row[i];
                 k++;
                 left = true;
               }

          }

       }

    StringGrid1->RowCount--;
    mysql_free_result(res);
    IsVariables = true;
    return true;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::nice_time(AnsiString buff)
{

  unsigned long sec;
  unsigned long tmp;
  AnsiString  mytime;

  sec = StrToInt(buff);

  if (sec >= 3600L*24)
  {
    tmp=sec/(3600L*24);
    sec-=3600L*24*tmp;

    mytime = IntToStr(tmp);
    if (tmp > 1)
      mytime+= " days ";
    else
      mytime+= " day ";

  }

  if (sec >= 3600L)
  {
    tmp=sec/3600L;
    sec-=3600L*tmp;
     mytime += IntToStr(tmp);
    if (tmp > 1)
      mytime+= " hours ";
    else
      mytime+= " hour ";
  }
  if (sec >= 60)
  {
    tmp=sec/60;
    sec-=60*tmp;
    mytime += IntToStr(tmp);
    mytime+= " min ";

  }
  mytime += IntToStr(sec);
  mytime+= " sec ";
  st29->Text = mytime ;
 return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::Button11Click(TObject *Sender)
{
  if (IsConnect)
  {
    if (GetVariables())
     StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button10Click(TObject *Sender)
{
 if (IsConnect)
  {
    if (GetProcess())
     StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button6Click(TObject *Sender)
{
  if (IsConnect)
   {
    if (mysql_refresh(MySQL,REFRESH_HOSTS))
       StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button7Click(TObject *Sender)
{
  if (IsConnect)
  {
    if (mysql_refresh(MySQL,REFRESH_LOG))
     StatusLine->SimpleText = "";
  }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button8Click(TObject *Sender)
{
  if (IsConnect)
  {
    if (mysql_refresh(MySQL,REFRESH_TABLES))
      StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button2Click(TObject *Sender)
{
 Memo1->Enabled = true;
  Memo1->Lines->Clear();
  AnsiString asFileName = FileSearch("my.ini", TheWinDir());
  if (asFileName.IsEmpty())
   Application->MessageBox("Don't found my.ini file on the Win Directory", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
  else
   FillMyIni();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button3Click(TObject *Sender)
{
 TIniFile *pIniFile = new
 TIniFile(TheWinDir() + "\\my.ini");

 if (!Memo1->GetTextLen())
   Application->MessageBox("The Memo Box is Empty", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
 else
  {
   if(Application->MessageBox("Are you sure to write the modifications into My.ini file.", "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
    {
      Memo1->Lines->SaveToFile(TheWinDir() + "\\my.ini");

      Memo1->Lines->Clear();
      Memo1->Enabled = true;
      Memo1->Lines->Clear();
      if (NtVer->Checked)
       pIniFile->WriteString("WinMySQLadmin","Server",TheDir() + "/bin/mysqld-nt.exe");
      if (MysqldVer->Checked == true)
       pIniFile->WriteString("WinMySQLadmin","Server", TheDir() + "/bin/mysqld.exe");
      if (ShareVer->Checked)
       pIniFile->WriteString("WinMySQLadmin","Server",TheDir() + "/bin/mysqld-shareware.exe");
      if (OptVer->Checked)
       pIniFile->WriteString("WinMySQLadmin","Server", TheDir() + "/bin/mysqld-opt.exe");
      FillMyIni();
      Application->MessageBox("My.ini was modificated", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
    }

  }
  delete pIniFile;
  Memo1->Lines->Clear();
  FillMyIni();

}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button1Click(TObject *Sender)
{
  if(CreatingShortCut())
   Application->MessageBox("The ShortCut on Start Menu was created", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
 else
   Application->MessageBox("Fails the Operation of Create the ShortCut", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton2Click(TObject *Sender)
{
 BROWSEINFO    info;
  char          szDir[MAX_PATH];
  char          szDisplayName[MAX_PATH];
  LPITEMIDLIST  pidl;
  LPMALLOC      pShellMalloc;


 if(SHGetMalloc(&pShellMalloc) == NO_ERROR)
    {

      memset(&info, 0x00,sizeof(info));
      info.hwndOwner = Handle;
      info.pidlRoot  = 0;
      info.pszDisplayName = szDisplayName;
      info.lpszTitle = "Search MySQL Base Directory";
      info.ulFlags   = BIF_RETURNONLYFSDIRS;
      info.lpfn = 0;

      pidl = SHBrowseForFolder(&info);

      if(pidl)
        {

          if(SHGetPathFromIDList(pidl, szDir)) {BaseDir->Text = szDir; }

            pShellMalloc->Free(pidl);
        }
        pShellMalloc->Release();
    }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Button4Click(TObject *Sender)
{
 if (IsConnect)
  {
    Memo3->Lines->Add(mysql_stat(MySQL));
  }
}
//---------------------------------------------------------------------------


void __fastcall TForm1::SpeedButton3Click(TObject *Sender)
{
  if(Showme1->Caption == "Show me")  {  TrayMessage(NIM_DELETE);
       Showme1->Caption = "Hide me";  Show();  }
  else  { TrayMessage(NIM_ADD);  TrayMessage(NIM_MODIFY);
       Showme1->Caption = "Show me";  Hide();  }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::ExtendedClick(TObject *Sender)
{
if (ya)
 {
 Extended->Caption = "Start Extended Server Status";
 ya = false;
 ClearBox();
 }
else
 {
 Extended->Caption = "Stop Extended Server Status";
 ya = true;
 }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetServerOptions(void)
{
AnsiString FileName;
FileName =  FileSearch("mysqld-opt.exe", ExtractFilePath(Application->ExeName));
if (FileName.IsEmpty()) {OptVer->Enabled = false; }

FileName =  FileSearch("mysqld-shareware.exe", ExtractFilePath(Application->ExeName));
if (FileName.IsEmpty()) {ShareVer->Enabled = false; }

FileName =  FileSearch("mysqld.exe", ExtractFilePath(Application->ExeName));
if (FileName.IsEmpty()) {MysqldVer->Enabled = false; }

FileName =  FileSearch("mysqld-nt.exe", ExtractFilePath(Application->ExeName));
if (FileName.IsEmpty()) {NtVer->Enabled = false; }

}
//---------------------------------------------------------------------------
void __fastcall TForm1::GetReportServer(void)
{

  AnsiString strspace;
  Memo5->Lines->Clear();
  Memo5->Lines->Add("This Report was made using the WinMySQLadmin 1.0 Tool");
  Memo5->Lines->Add("");
  Memo5->Lines->Add(Now());
  Memo5->Lines->Add("");

   preport = true;
   Memo5->Lines->Add("");
   Memo5->Lines->Add("Server Status Values");
   Memo5->Lines->Add("");
   Memo5->Lines->Add(GetString("Server Info") + mysql_get_server_info(MySQL));
   Memo5->Lines->Add(GetString("Host Info") + mysql_get_host_info(MySQL));
   Memo5->Lines->Add(GetString("Client Info") + mysql_get_client_info());
   Memo5->Lines->Add(GetString("Proto Info") + mysql_get_proto_info(MySQL));
   GetExtendedStatus();
   preport = false;
   treport = true;
   Memo5->Lines->Add("");
   Memo5->Lines->Add("Variables Values");
   Memo5->Lines->Add("");
   GetVariables();
   treport = false;
   ereport = true;
   Memo5->Lines->Add("");
   Memo5->Lines->Add("Last Lines from Err File");
   Memo5->Lines->Add("");
   SeekErrFile();
   ereport = false;

}

void __fastcall TForm1::SpeedButton4Click(TObject *Sender)
{
 if(IsConnect)
  GetReportServer();
 else
  Application->MessageBox("The Server must be connected", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton5Click(TObject *Sender)
{
  AnsiString PathName;
  SaveFileDialog->FileName = PathName;
  if (SaveFileDialog->Execute() ){
    PathName= SaveFileDialog->FileName;
    Caption = ExtractFileName(PathName);
    Memo5->Lines->SaveToFile(PathName);
    Memo5->Modified = false;
  }
}
//---------------------------------------------------------------------------
String  __fastcall TForm1::GetString(String k)
{
  int i = 35 - k.Length();
  for (int y = 1 ; y <= i ;y++ )
     k+= " ";
   return k ;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::SpeedButton6Click(TObject *Sender)
{
 PrinterSetupDialog1->Execute();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton7Click(TObject *Sender)
{
  AnsiString PathName;
  if (PrintDialog1->Execute()){
    try {
        Memo5->Print(PathName);
    }
    catch(...){
        Printer()->EndDoc();
        throw;
    }
  }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton8Click(TObject *Sender)
{
 Memo5->CutToClipboard();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton9Click(TObject *Sender)
{
  Memo5->CopyToClipboard();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton10Click(TObject *Sender)
{

 Memo5->PasteFromClipboard();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton11Click(TObject *Sender)
{
 Memo5->ClearSelection();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::SpeedButton12Click(TObject *Sender)
{
 Memo5->SelectAll();
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::GetMainRoot()
{

 MYSQL_RES *res;
 MYSQL_ROW row;
 unsigned int i;
 AnsiString command;

 CleanGrid();
 CleanGridI();
 TakeIP();

 MySQLNode = DBView->Items->Add(NULL, mainroot.UpperCase());
 MySQLNode->ImageIndex = 0;

 if (!(res=mysql_list_dbs(MySQL,"%")))  { return false; }
   while ((row=mysql_fetch_row(res)) != 0) {
     mysql_field_seek(res,0);

     for (i=0 ; i < mysql_num_fields(res); i++)
      {
        MySQLDbs = DBView->Items->AddChild(MySQLNode, row[i]);
        MySQLDbs->ImageIndex = 1;
        MySQLDbs->SelectedIndex = 1;


      }

   }

   mysql_free_result(res);
   MySQLNode->Expanded = true;




 IsMySQLNode = true;
 return true;

}
//---------------------------------------------------------------------------
void __fastcall TForm1::DeleteDatabaseSClick(TObject *Sender)
{
 AnsiString alert;
 if (IsConnect)
  {
   if(DBView->Selected == MySQLNode )
    Application->MessageBox("Invalid database row selected.", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
   else if ( DBView->Selected == NULL )
    Application->MessageBox("Invalid database row selected.", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
   else
    {
     if (DBView->Selected->Text.UpperCase() == "MYSQL")
      Application->MessageBox("You cann't use this tool to drop the MySQL Database.", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
     else {
       alert = "Are you sure to drop the < ";
       alert+= DBView->Selected->Text.c_str();
       alert+= " > database.";
      if(Application->MessageBox(alert.c_str(), "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
       {
         char* lese = DBView->Selected->Text.c_str();
        if (!mysql_drop_db(MySQL, lese ))
          {
            DBView->Items->Clear();
            GetMainRoot();
           }
         else
           Application->MessageBox("Fails to drop the Database.", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
        }
      }
   }
  }
 else
   Application->MessageBox("The Server must be connected", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
}
//---------------------------------------------------------------------------
 bool __fastcall TForm1::IsDatabase(String Name)
{
  MYSQL_RES *res;
  MYSQL_ROW row;
  unsigned int i;
  AnsiString command;


  CleanTree();
  command = "use ";
  command+= Name.c_str();
  char* das = command.c_str();
  char* lis = Name.c_str();
  if (mysql_query(MySQL, das ) ||
  !(res=mysql_list_tables(MySQL,"%")))
  return false;

  MySQLNodeT = TableView->Items->Add(NULL, lis);
  MySQLNodeT->ImageIndex = 1;
  MySQLNodeT->SelectedIndex = 1;
  while ((row=mysql_fetch_row(res)) != 0) {
  mysql_field_seek(res,0);

  for (i=0 ; i < mysql_num_fields(res); i++)
   {

     MySQLTbs = TableView->Items->AddChild(MySQLNodeT, row[i]);
     MySQLTbs->ImageIndex = 2;
     MySQLTbs->SelectedIndex = 2;
   }
   MySQLNodeT->Expanded = true;
  }
  mysql_free_result(res);
  return true;
}
//---------------------------------------------------------------------------


void __fastcall TForm1::DBViewClick(TObject *Sender)
{

 if (IsConnect)
  {
    if (DBView->Selected != MySQLNode && DBView->Selected != NULL  )
     {
       IsDatabase(DBView->Selected->Text);

     }
    else
     {
      CleanTree();
     }
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::TableViewClick(TObject *Sender)
{
 if (IsConnect)
  {
    if (DBView->Selected != MySQLNodeT )
     {
       IsTable(TableView->Selected->Text);
       IsIndex(TableView->Selected->Text);

     }
    else
     {
      CleanGrid();
      CleanGridI();

     }
  }
}
//---------------------------------------------------------------------------
 bool __fastcall TForm1::IsTable(String Name)
{
 MYSQL_RES *res;
 MYSQL_ROW row;
 unsigned int i;
 int k = 0;
 int therow = 1;
 new_line=1;
 AnsiString command;
 AnsiString commandt;

 CleanGrid();
 CleanGridI();
 command = "use ";
 command+= DBView->Selected->Text.c_str();
 char* las = command.c_str();

 commandt = "desc ";
 commandt+= Name.c_str();
 char* les = commandt.c_str();

 if (mysql_query(MySQL, las ))
 return false;

 if (mysql_query(MySQL, les ) ||
 !(res=mysql_store_result(MySQL)))
 return false ;

 StringGrid4->Cells[0][0] = "Field";
 StringGrid4->Cells[1][0] = "Type";
 StringGrid4->Cells[2][0] = "Null";
 StringGrid4->Cells[3][0] = "Key";
 StringGrid4->Cells[4][0] = "Default";
 StringGrid4->Cells[5][0] = "Extra";
 StringGrid4->Cells[6][0] = "Previleges";


 int thecounter;
 String u = GetNumberServer();
 if ( u == "3.22")
  {
   StringGrid3->ColCount = 7;
   thecounter = 4;
  }
 else
  thecounter = 5;

 while ((row=mysql_fetch_row(res)) != 0)
 {
   mysql_field_seek(res,0);

   for (i=0 ; i < mysql_num_fields(res); i++)
    {
     if (k <= thecounter )
     {
       StringGrid4->Cells[k][therow] = row[i];
       k++;
     }
     else
     {
       StringGrid4->Cells[(k)][therow] = row[i];
       k = 0;
       therow++ ;
       StringGrid4->RowCount++;
      }
    }

 }
 StringGrid4->RowCount--;
 mysql_free_result(res);
 return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::TableViewChange(TObject *Sender, TTreeNode *Node)
{
if (IsConnect)
  {
    if (DBView->Selected != MySQLNodeT )
     {
       IsTable(TableView->Selected->Text);
       IsIndex(TableView->Selected->Text);

     }
    else
     {
      CleanGrid();
      CleanGridI();

     }
  }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::DBViewChange(TObject *Sender, TTreeNode *Node)
{
 if (IsConnect)
  {
    if (DBView->Selected != MySQLNode )
     {
       IsDatabase(DBView->Selected->Text);

     }
    else
     {
      CleanTree();
     }
  }

}
//---------------------------------------------------------------------------
void __fastcall TForm1::RefreshSClick(TObject *Sender)
{
 MYSQL_RES *res;
 MYSQL_ROW row;
 unsigned int i;
 AnsiString command;

 if (IsConnect)
  {
   IsMySQLNode = false;
   CleanTree();
   DBView->Items->Clear();

   TakeIP();

   MySQLNode = DBView->Items->Add(NULL, mainroot.UpperCase());
   MySQLNode->ImageIndex = 0;

   if (!(res=mysql_list_dbs(MySQL,"%")))  { /*do nothing;*/ }
    while ((row=mysql_fetch_row(res)) != 0) {
    mysql_field_seek(res,0);

     for (i=0 ; i < mysql_num_fields(res); i++)
      {
        MySQLDbs = DBView->Items->AddChild(MySQLNode, row[i]);
        MySQLDbs->ImageIndex = 1;
        MySQLDbs->SelectedIndex = 1;

      }

   }

   mysql_free_result(res);

 IsMySQLNode = true;

 MySQLNode->Expanded = true;

  }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::CreateDatabaseSClick(TObject *Sender)
{

 if (IsConnect)
  {
   dbfrm->Show();

  }
 else
  ShowMessage("Precisa estar conectado");
}
//---------------------------------------------------------------------------
void __fastcall TForm1::CleanTree(void)
{
  StringGrid4->RowCount= 2;
  StringGrid4->Cells[0][1] = "";
  StringGrid4->Cells[1][1] = "";
  StringGrid4->Cells[2][1] = "";
  StringGrid4->Cells[3][1] = "";
  StringGrid4->Cells[4][1] = "";
  StringGrid4->Cells[5][1]  = "";
  TableView->Items->Clear();

}
//---------------------------------------------------------------------------
void __fastcall TForm1::CleanGrid(void)
{
  StringGrid4->RowCount= 2;
  StringGrid4->Cells[0][1] = "";
  StringGrid4->Cells[1][1] = "";
  StringGrid4->Cells[2][1] = "";
  StringGrid4->Cells[3][1] = "";
  StringGrid4->Cells[4][1] = "";
  StringGrid4->Cells[5][1]  = "";
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::CreatingDB()
{

 if (mysql_create_db(MySQL, dbfrm->Edit1->Text.c_str()))
  return true;
 else
  return false;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::OutRefresh(void)
{
 RefreshSClick(dbfrm->SpeedButton1);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FlushHosts1Click(TObject *Sender)
{
   if (IsConnect)
   {
    if (mysql_refresh(MySQL,REFRESH_HOSTS))
       StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FlushLogs1Click(TObject *Sender)
{
   if (IsConnect)
  {
    if (mysql_refresh(MySQL,REFRESH_LOG))
     StatusLine->SimpleText = "";
  }
}
//---------------------------------------------------------------------------

void __fastcall TForm1::FlushTables1Click(TObject *Sender)
{
  if (IsConnect)
  {
    if (mysql_refresh(MySQL,REFRESH_TABLES))
      StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
 bool __fastcall TForm1::IsIndex(String Name)
{
 MYSQL_RES *res;
 MYSQL_ROW row;
 unsigned int i;
 int k = 0;
 int therow = 1;
 new_line=1;
 AnsiString command;
 AnsiString commandt;
 i = 0;
 CleanGridI();
 command = "use ";
 command+= DBView->Selected->Text.c_str();
 char* las = command.c_str();

 commandt = "show index from ";
 commandt+= Name.c_str();
 char* les = commandt.c_str();

 if (mysql_query(MySQL, las ))
 return false;

 if (mysql_query(MySQL, les ) ||
 !(res=mysql_store_result(MySQL)))
 return false ;

 StringGrid3->RowCount= 2;
 StringGrid3->Cells[0][0] = "Table";
 StringGrid3->Cells[1][0] = "Non_unique";
 StringGrid3->Cells[2][0] = "Key_name";
 StringGrid3->Cells[3][0] = "Seq_in_index";
 StringGrid3->Cells[4][0] = "Col_name";
 StringGrid3->Cells[5][0] = "Collation";
 StringGrid3->Cells[6][0] = "Card.";
 StringGrid3->Cells[7][0] = "Sub_part";
 StringGrid3->Cells[8][0] = "Packed";
 StringGrid3->Cells[9][0] = "Comment";

 int thecounter;
 String u = GetNumberServer();

 if ( u == "3.22")
  {
   StringGrid3->ColCount = 8;
   thecounter = 6;
  }
 else
  thecounter = 8;
 while ((row=mysql_fetch_row(res)) != 0)
 {
   mysql_field_seek(res,0);

   for (i=0 ; i < mysql_num_fields(res); i++)
    {
     if (k <= thecounter )
     {
       StringGrid3->Cells[k][therow] = row[i];
       k++;
     }
     else
     {
       StringGrid3->Cells[(k)][therow] = row[i];
       k = 0;
       therow++ ;
       StringGrid3->RowCount++;
      }
    }

 }
 if (i)
 StringGrid3->RowCount--;
 mysql_free_result(res);
 return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::CleanGridI(void)
{
  StringGrid3->RowCount= 2;
  StringGrid3->Cells[0][1] = "";
  StringGrid3->Cells[1][1] = "";
  StringGrid3->Cells[2][1] = "";
  StringGrid3->Cells[3][1] = "";
  StringGrid3->Cells[4][1] = "";
  StringGrid3->Cells[5][1]  = "";
  StringGrid3->Cells[6][1]  = "";
  StringGrid3->Cells[7][1]  = "";
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::CreatingTable(String TheTable)
{

 if (!mysql_query(MySQL, TheTable.c_str()))
  return true;
 else
  return false;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::GetExtendedStatus()
{
  if (!ya && !preport)
   return true;

  MYSQL_RES *res;
  MYSQL_ROW row;
  unsigned int i;
  int k = 1;
  new_line=1;
  bool left = true;
  bool open_tables = false;
  bool open_files = false;
  bool uptime = false;
  bool running_threads = false;
  bool open_streams = false;
  bool slow_queries = false;
  bool opened_tables = false;
  bool questions = false;

  AnsiString report;
  if (yy)
  StringGrid5->RowCount = 2;

   if (mysql_query(MySQL,"show status") ||
	  !(res=mysql_store_result(MySQL)))
      {
        return false;
      }

      while ((row=mysql_fetch_row(res)) != 0)
       {
         mysql_field_seek(res,0);

         StringGrid5->Cells[0][0] = "Variable Name";
         StringGrid5->Cells[1][0] = "Value";


         for (i=0 ; i < mysql_num_fields(res); i++)
          {

             if (left)
              {
                 if (preport)
                 report = GetString(row[i]);
                 if ( (String) row[i] ==  "Open_tables")
                   open_tables = true;
                 else
                   open_tables = false;
                 if ( (String) row[i] ==  "Open_files")
                   open_files = true;
                 else
                   open_files = false;
                 if ((String) row[i] == "Uptime")
                   uptime = true;
                 else
                   uptime = false;

                 if ( (String) row[i] == "Opened_tables")
                    opened_tables = true;
                 else
                    opened_tables = false;

                 if ( (String) row[i] == "Threads_running" || (String) row[i] == "Running_threads")
                    running_threads = true;
                 else
                    running_threads = false;

                 if ( (String) row[i] == "Open_streams")
                    open_streams = true;
                 else
                    open_streams = false;

                 if ( (String) row[i] == "Slow_queries")
                    slow_queries = true;
                 else
                    slow_queries = false;

                 if ( (String) row[i] == "Questions")
                    questions = true;
                 else
                    questions = false;

                 if (yy)
                 StringGrid5->Cells[0][k++] = row[i];

                 left = false;
              }
             else
               {
                 if (preport)
                  Memo5->Lines->Add(report + row[i]);
                 if (open_tables)
                  st22->Text = row[i];
                 if (open_files)
                  st23->Text = row[i];
                 if (uptime)
                  nice_time(row[i]);
                 if (running_threads)
                  st27->Text = row[i];
                 if (open_streams)
                  st24->Text = row[i];
                 if (slow_queries)
                  st28->Text = row[i];
                 if (opened_tables)
                  st25->Text = row[i];
                 if (questions){
                   q++;
                   st26->Text = StrToInt64(row[i]) - q; }

                 if (yy){
                 StringGrid5->RowCount++;
                 StringGrid5->Cells[1][--k] = row[i];
                 k++; }

                 left = true;
               }

          }

       }


    if (rinit)
     StringGrid5->RowCount--;
    mysql_free_result(res);
    yy = false;
    return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::SpeedButton13Click(TObject *Sender)
{
  yy = true;
 // rinit = true;
}
//---------------------------------------------------------------------------
String  __fastcall TForm1::GetNumberServer()
{
 String TheVersion;

 TheVersion  =  mysql_get_server_info(MySQL) ;
 TheVersion.SetLength(4);
 return TheVersion;


}

//---------------------------------------------------------------------------
void __fastcall TForm1::KillProcess1Click(TObject *Sender)
{

  if (IsConnect)
    KillPID();
  else
    Application->MessageBox("The Server must be connected", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::KillPID()
{
  String s  = "Are you sure to kill the process PID no. ";
  s+= StringGrid2->Cells[0][StringGrid2->Row];
  s+= " of the USER ";
  s+= StringGrid2->Cells[1][StringGrid2->Row];
  unsigned long  xx = mysql_thread_id(MySQL);
  unsigned long  yy = StrToInt(StringGrid2->Cells[0][StringGrid2->Row]);
  if ( xx != yy)
   {
     if(Application->MessageBox(s.c_str(), "WinMySQLadmin 1.0", MB_YESNOCANCEL | MB_ICONQUESTION	) == IDYES)
      {
        if (!mysql_kill(MySQL,yy))
         {
          GetProcess();
          return true;
         }
      }
    }
  else
   {
     Application->MessageBox("From here you can't kill the PID of this tool", "WinMySQLadmin 1.0", MB_OK | MB_ICONEXCLAMATION);
     return true;
   }
 return true;
}
void __fastcall TForm1::FlushThreads1Click(TObject *Sender)
{
 if (IsConnect)
  {
    if (mysql_refresh(MySQL,REFRESH_THREADS))
      StatusLine->SimpleText = "";
   }
}
//---------------------------------------------------------------------------

