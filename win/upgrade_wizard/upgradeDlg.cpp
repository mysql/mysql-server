
// upgradeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "upgrade.h"
#include "upgradeDlg.h"
#include "windows.h"
#include "winsvc.h"
#include <msi.h>
#pragma comment(lib, "msi")
#pragma comment(lib, "version")
#include <map>
#include <string>
#include <vector>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define PRODUCT_NAME "MariaDB"

// CUpgradeDlg dialog

CUpgradeDlg::CUpgradeDlg(CWnd* pParent /*=NULL*/)
  : CDialog(CUpgradeDlg::IDD, pParent)
{
  m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUpgradeDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_LIST1, m_Services);
  DDX_Control(pDX, IDC_PROGRESS1, m_Progress);
  DDX_Control(pDX, IDOK, m_Ok);
  DDX_Control(pDX, IDCANCEL, m_Cancel);
  DDX_Control(pDX, IDC_EDIT1, m_IniFilePath);
  DDX_Control(pDX, IDC_EDIT2, m_DataDir);
  DDX_Control(pDX, IDC_EDIT3, m_Version);
  DDX_Control(pDX, IDC_EDIT7, m_IniFileLabel);
  DDX_Control(pDX, IDC_EDIT8, m_DataDirLabel);
  DDX_Control(pDX, IDC_EDIT9, m_VersionLabel);
  DDX_Control(pDX, IDC_BUTTON1, m_SelectAll);
  DDX_Control(pDX, IDC_BUTTON2, m_ClearAll);
}

BEGIN_MESSAGE_MAP(CUpgradeDlg, CDialog)
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
  ON_LBN_SELCHANGE(IDC_LIST1, &CUpgradeDlg::OnLbnSelchangeList1)
  ON_CONTROL(CLBN_CHKCHANGE, IDC_LIST1, OnChkChange)
  ON_BN_CLICKED(IDOK, &CUpgradeDlg::OnBnClickedOk)
  ON_BN_CLICKED(IDCANCEL, &CUpgradeDlg::OnBnClickedCancel)
  ON_BN_CLICKED(IDC_BUTTON1,&CUpgradeDlg::OnBnSelectAll)
  ON_BN_CLICKED(IDC_BUTTON2,&CUpgradeDlg::OnBnClearAll)
END_MESSAGE_MAP()


struct ServiceProperties
{
  string servicename;
  string myini;
  string datadir;
  string version;
};

vector<ServiceProperties> services;

/*
  Get version from an executable.
  Returned version is either major.minor.patch or
  <unknown> , of executable does not have any version
  info embedded (like MySQL 5.1 for example)
*/
string GetExeVersion(const string& filename, int *major, int *minor, int *patch)
{
  DWORD handle;
  *major= *minor= *patch= 0;

  DWORD size = GetFileVersionInfoSize(filename.c_str(), &handle);
  BYTE* versionInfo = new BYTE[size];
  if (!GetFileVersionInfo(filename.c_str(), handle, size, versionInfo))
  {
    delete[] versionInfo;
    return "<unknown>";
  }
  // we have version information
  UINT len = 0;
  VS_FIXEDFILEINFO*   vsfi = NULL;
  VerQueryValue(versionInfo, "\\", (void**)&vsfi, &len);
  char arr[64];

  *major= (int)HIWORD(vsfi->dwFileVersionMS);
  *minor= (int)LOWORD(vsfi->dwFileVersionMS);
  *patch= (int)HIWORD(vsfi->dwFileVersionLS);
  sprintf_s(arr,"%d.%d.%d", *major, *minor, *patch); 
  delete[] versionInfo;
  return string(arr);
}


void GetMyVersion(int *major, int *minor, int *patch)
{
  char path[MAX_PATH];
  *major= *minor= *patch =0;
  if (GetModuleFileName(NULL, path, MAX_PATH))
  {
    GetExeVersion(path, major, minor, patch);
  }
}
// CUpgradeDlg message handlers

/* Handle selection changes in services list */
void CUpgradeDlg::SelectService(int index)
{
  m_IniFilePath.SetWindowText(services[index].myini.c_str());
  m_DataDir.SetWindowText(services[index].datadir.c_str());
  m_Version.SetWindowText(services[index].version.c_str());
}


/* Remove quotes from string */
static char *RemoveQuotes(char *s)
{
  if(s[0]=='"')
  {
    s++;
    char *p= strchr(s, '"');
    if(p)
      *p= 0;
  }
  return s;
}


/*
  Iterate over services, lookup for mysqld.exe ones.
  Compare mysqld.exe version with current version, and display
  service if corresponding mysqld.exe has lower version.

  The version check is not strict, i.e we allow to "upgrade" 
  for the same major.minor combination. This can be useful for 
  "upgrading" from 32 to 64 bit, or for MySQL=>Maria conversion.
*/
void CUpgradeDlg::PopulateServicesList()
{

  SC_HANDLE scm = OpenSCManager(NULL, NULL, 
    SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT); 
  if (scm == NULL) 
  { 
    ErrorExit("OpenSCManager failed");
  }

  static BYTE buf[64*1024];
  static BYTE configBuffer[8*1024];
  char datadirBuf[MAX_PATH];
  char datadirNormalized[MAX_PATH];
  DWORD bufsize= sizeof(buf);
  DWORD bufneed;
  DWORD num_services;
  BOOL ok= EnumServicesStatusEx(scm, SC_ENUM_PROCESS_INFO,  SERVICE_WIN32,
    SERVICE_STATE_ALL,  buf, bufsize,  &bufneed, &num_services, NULL, NULL);
  if(!ok) 
    ErrorExit("EnumServicesStatusEx failed");


  LPENUM_SERVICE_STATUS_PROCESS info =
    (LPENUM_SERVICE_STATUS_PROCESS)buf;
  int index=-1;
  for (ULONG i=0; i < num_services; i++)
  {
    SC_HANDLE service= OpenService(scm, info[i].lpServiceName, 
      SERVICE_QUERY_CONFIG);
    if (!service)
      continue;
    QUERY_SERVICE_CONFIGW *config= 
      (QUERY_SERVICE_CONFIGW*)(void *)configBuffer;
    DWORD needed;
    BOOL ok= QueryServiceConfigW(service, config,sizeof(configBuffer), &needed);
    CloseServiceHandle(service);
    if (ok)
    {
      int argc;
      wchar_t **wargv = CommandLineToArgvW(config->lpBinaryPathName, &argc);

      // We expect  path\to\mysqld --defaults-file=<path> <servicename>
      if(argc == 3)  
      {
        
         // Convert wide strings to ANSI 
        char *argv[3];
        for(int k=0; k < 3;k++)
        {
          size_t nbytes = 2*wcslen(wargv[k])+1; 
          argv[k]= new char[nbytes];
          wcstombs(argv[k], wargv[k], nbytes);
        }

        size_t len= strlen(argv[0]);
        char path[MAX_PATH]={0};
        char *filepart;
        GetFullPathName(argv[0],MAX_PATH, path, &filepart);
        if(_stricmp(filepart, "mysqld.exe") == 0 ||
          _stricmp(filepart, "mysqld") == 0)
        {
          if(_strnicmp(argv[1],"--defaults-file=",16) == 0)
          {
            /* Remove quotes around defaults-file */
            char *inifile= argv[1] + 16;
            inifile = RemoveQuotes(inifile);

            char *datadir=datadirBuf;
            GetPrivateProfileString("mysqld", "datadir", NULL, datadirBuf,
              MAX_PATH, inifile);

            /* Remove quotes from datadir */
            datadir= RemoveQuotes(datadir);

            GetFullPathName(datadir, MAX_PATH, datadirNormalized, NULL);
            ServiceProperties props;

            props.myini = inifile;
            props.servicename = info[i].lpServiceName;
            string exefilename(argv[0]);
            if(!strstr(argv[0], ".exe"))
              exefilename += ".exe";
            int major, minor, patch;
            props.version= GetExeVersion(exefilename, &major, &minor, &patch);
            if(m_MajorVersion > major || 
              (m_MajorVersion == major && m_MinorVersion >= minor))
            {
              if (_strnicmp(exefilename.c_str(), m_InstallDir.c_str(),
                m_InstallDir.size()) != 0)
              {
                props.datadir = datadirNormalized;
                index = m_Services.AddString(info[i].lpServiceName);
                services.resize(index+1);
                services[index] = props;
              }
            }
          }
        }
        for(int k=0; k< 3;k++)
          delete[] argv[k];
      }
      LocalFree((HLOCAL)wargv);
    }
    if (index != -1)
    {
      m_Services.SetCurSel(0);
      SelectService(m_Services.GetCurSel());
    }
  }
  if (services.size())
  {
    SelectService(0);
  }
  else
  {
    char message[128];
    sprintf(message, 
      "There is no service that can be upgraded to " PRODUCT_NAME " %d.%d.%d",
      m_MajorVersion, m_MinorVersion, m_PatchVersion);
    MessageBox(message, PRODUCT_NAME " Upgrade Wizard", MB_ICONINFORMATION);
    exit(0);
  }
  if(scm)
    CloseServiceHandle(scm);
}

BOOL CUpgradeDlg::OnInitDialog()
{
  CDialog::OnInitDialog();
  m_UpgradeRunning= FALSE;
  // Set the icon for this dialog.  The framework does this automatically
  //  when the application's main window is not a dialog
  SetIcon(m_hIcon, TRUE);			// Set big icon
  SetIcon(m_hIcon, FALSE);		// Set small icon
  m_Ok.SetWindowText("Upgrade");
  m_DataDirLabel.SetWindowText("Data directory:");
  m_IniFileLabel.SetWindowText("Configuration file:");
  m_VersionLabel.SetWindowText("Version:");

  char myFilename[MAX_PATH];
  GetModuleFileName(NULL, myFilename, MAX_PATH);
  char *p= strrchr(myFilename,'\\');
  if(p)
    p[1]=0;
  m_InstallDir= myFilename;

  GetMyVersion(&m_MajorVersion, &m_MinorVersion, &m_PatchVersion);
  char windowTitle[64];

  sprintf(windowTitle, PRODUCT_NAME " %d.%d.%d Upgrade Wizard",
    m_MajorVersion, m_MinorVersion, m_PatchVersion);
  SetWindowText(windowTitle);

  m_JobObject= CreateJobObject(NULL, NULL);

  /*
    Make all processes associated with the job terminate when the
    last handle to the job is closed or job is teminated.
  */
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
  jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  SetInformationJobObject(m_JobObject, JobObjectExtendedLimitInformation,
                              &jeli, sizeof(jeli));
  if(!AssignProcessToJobObject(m_JobObject, GetCurrentProcess()))
    ErrorExit("AssignProcessToJobObject failed");

  m_Progress.ShowWindow(SW_HIDE);
  m_Ok.EnableWindow(FALSE);
  PopulateServicesList();
  return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CUpgradeDlg::OnPaint()
{
  if (IsIconic())
  {
    CPaintDC dc(this); // device context for painting

    SendMessage(WM_ICONERASEBKGND, 
      reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

    // Center icon in client rectangle
    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    CRect rect;
    GetClientRect(&rect);
    int x = (rect.Width() - cxIcon + 1) / 2;
    int y = (rect.Height() - cyIcon + 1) / 2;

    // Draw the icon
    dc.DrawIcon(x, y, m_hIcon);
  }
  else
  {
    CDialog::OnPaint();
  }
}

// The system calls this function to obtain the cursor to display while the user
//  drags the minimized window.
HCURSOR CUpgradeDlg::OnQueryDragIcon()
{
  return static_cast<HCURSOR>(m_hIcon);
}


void CUpgradeDlg::OnLbnSelchangeList1()
{
  SelectService(m_Services.GetCurSel());
}

void CUpgradeDlg::OnChkChange()
{
  if(m_Services.GetCheck( m_Services.GetCurSel()))
  {
    GetDlgItem(IDOK)->EnableWindow();
  }
  else
  {
    for(int i=0; i< m_Services.GetCount(); i++)
    {
      if(m_Services.GetCheck(i))
        return;
    }
    // all items unchecked, disable OK button
    GetDlgItem(IDOK)->EnableWindow(FALSE);
  }
}



void CUpgradeDlg::ErrorExit(LPCSTR str)
{
  MessageBox(str, "Fatal Error", MB_ICONERROR);
  exit(1);
}


const int MAX_MESSAGES=512;

/* Main thread of the child process */
static HANDLE hChildThread;

void CUpgradeDlg::UpgradeOneService(const string& servicename)
{
  static string allMessages[MAX_MESSAGES];
  static char npname[MAX_PATH];
  static char pipeReadBuf[1];
  SECURITY_ATTRIBUTES saAttr;
  STARTUPINFO si={0};
  PROCESS_INFORMATION pi;
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
  saAttr.bInheritHandle = TRUE; 
  saAttr.lpSecurityDescriptor = NULL;

  HANDLE hPipeRead, hPipeWrite;
  if(!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 1))
    ErrorExit("CreateNamedPipe failed");

  /* Make sure read end of the pipe is not inherited */
  if (!SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0) )
    ErrorExit("Stdout SetHandleInformation"); 

  string commandline("mysql_upgrade_service.exe --service=");
  commandline += servicename;
  si.cb = sizeof(si);
  si.hStdInput= GetStdHandle(STD_INPUT_HANDLE);
  si.hStdOutput= hPipeWrite;
  si.hStdError= hPipeWrite;
  si.wShowWindow= SW_HIDE;
  si.dwFlags= STARTF_USESTDHANDLES |STARTF_USESHOWWINDOW;
  if (!CreateProcess(NULL, (LPSTR)commandline.c_str(), NULL, NULL, TRUE,
    0, NULL, NULL, &si, &pi))
  {
    string errmsg("Create Process ");
    errmsg+= commandline;
    errmsg+= " failed";
    ErrorExit(errmsg.c_str());
  }
  hChildThread = pi.hThread;
  DWORD nbytes;
  bool newline= false;
  int lines=0;
  CloseHandle(hPipeWrite);

  string output_line;
  while(ReadFile(hPipeRead, pipeReadBuf, 1, &nbytes, NULL))
  {
    if(pipeReadBuf[0] == '\n')
    {
      allMessages[lines%MAX_MESSAGES] = output_line;
      m_DataDir.SetWindowText(allMessages[lines%MAX_MESSAGES].c_str());
      output_line.clear();
      lines++;

      /* 
        Updating progress dialog.There are currently 9 messages from 
        mysql_upgrade_service (actually it also writes Phase N/M but 
        we do not parse
      */
#define EXPRECTED_MYSQL_UPGRADE_MESSAGES 9

      int stepsTotal= m_ProgressTotal*EXPRECTED_MYSQL_UPGRADE_MESSAGES;
      int stepsCurrent= m_ProgressCurrent*EXPRECTED_MYSQL_UPGRADE_MESSAGES
        + lines;
      int percentDone= stepsCurrent*100/stepsTotal;
      m_Progress.SetPos(percentDone);
    }
    else
    {
      if(pipeReadBuf[0] != '\r')
       output_line.push_back(pipeReadBuf[0]);
    }
  }
  CloseHandle(hPipeWrite);

  if(WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0)
    ErrorExit("WaitForSingleObject failed");
  DWORD exitcode;
  if (!GetExitCodeProcess(pi.hProcess, &exitcode))
    ErrorExit("GetExitCodeProcess failed");

  if (exitcode != 0)
  {
    string  errmsg= "mysql_upgrade_service returned error for service ";
    errmsg += servicename;
    errmsg += ":\r\n";
    errmsg+= output_line;
    ErrorExit(errmsg.c_str());
  }
  CloseHandle(pi.hProcess);
  hChildThread= 0;
  CloseHandle(pi.hThread);
}


void CUpgradeDlg::UpgradeServices()
{

  /*
    Disable some dialog items during upgrade (OK button,
    services list)
  */
  m_Ok.EnableWindow(FALSE);
  m_Services.EnableWindow(FALSE);
  m_SelectAll.EnableWindow(FALSE);
  m_ClearAll.EnableWindow(FALSE);

  /*
    Temporarily repurpose IniFileLabel/IniFilePath and
    DatDirLabel/DataDir controls to show progress messages.
  */
  m_VersionLabel.ShowWindow(FALSE);
  m_Version.ShowWindow(FALSE);
  m_Progress.ShowWindow(TRUE);
  m_IniFileLabel.SetWindowText("Converting service:");
  m_IniFilePath.SetWindowText("");
  m_DataDirLabel.SetWindowText("Progress message:");
  m_DataDir.SetWindowText("");


  m_ProgressTotal=0;
  for(int i=0; i< m_Services.GetCount(); i++)
  {
    if(m_Services.GetCheck(i))
      m_ProgressTotal++;
  }
  m_ProgressCurrent=0;
  for(int i=0; i< m_Services.GetCount(); i++)
  {
    if(m_Services.GetCheck(i))
    {
      m_IniFilePath.SetWindowText(services[i].servicename.c_str());
      m_Services.SelectString(0, services[i].servicename.c_str());
      UpgradeOneService(services[i].servicename);
      m_ProgressCurrent++;
    }
  }

  MessageBox("Service(s) successfully upgraded", "Success", 
    MB_ICONINFORMATION);

  /* Rebuild services list */
  vector<ServiceProperties> new_instances;
  for(int i=0; i< m_Services.GetCount(); i++)
  {
    if(!m_Services.GetCheck(i))
      new_instances.push_back(services[i]);
  }

  services= new_instances;
  m_Services.ResetContent();
  for(size_t i=0; i< services.size();i++)
    m_Services.AddString(services[i].servicename.c_str());
  if(services.size())
  {
    m_Services.SelectString(0,services[0].servicename.c_str());
    SelectService(0);
  }
  else
  {
    /* Nothing to do, there are no upgradable services */
    exit(0);
  }

  /*
    Restore controls that were temporarily repurposed for
    progress info to their normal state
  */
  m_IniFileLabel.SetWindowText("Configuration file:");
  m_DataDirLabel.SetWindowText("Data Directory:");
  m_VersionLabel.ShowWindow(TRUE);
  m_Version.ShowWindow(TRUE);
  m_Progress.SetPos(0);
  m_Progress.ShowWindow(FALSE);

  /* Re-enable controls */
  m_Ok.EnableWindow(TRUE);
  m_Services.EnableWindow(TRUE);
  m_SelectAll.EnableWindow(TRUE);
  m_ClearAll.EnableWindow(TRUE);

  m_UpgradeRunning= FALSE;
}


/* Thread procedure for upgrade services operation */
static UINT UpgradeServicesThread(void *param)
{
  CUpgradeDlg *dlg= (CUpgradeDlg *)param;
  dlg->UpgradeServices();
  return 0;
}


/*
  Do upgrade  for all services currently selected
  in the list. Since it is a potentially lengthy operation that 
  might block it has to be done in a background thread.
*/
void CUpgradeDlg::OnBnClickedOk()
{
  if(m_UpgradeRunning)
    return;
  m_UpgradeRunning= TRUE;
  AfxBeginThread(UpgradeServicesThread, this);
}


/* 
  Cancel button clicked.
  If upgrade is running, suspend mysql_upgrade_service, 
  and ask user whether he really wants to stop.Terminate
  upgrade wizard and all subprocesses if users wants it.

  If upgrade is not running, terminate the Wizard
*/
void CUpgradeDlg::OnBnClickedCancel()
{
  if(m_UpgradeRunning)
  {
    bool suspended = (SuspendThread(hChildThread) != (DWORD)-1);
    int ret = MessageBox(
      "Upgrade is in progress. Are you sure you want to terminate?",
      0, MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION);
    if(ret != IDYES)
    {
      if(suspended)
        ResumeThread(hChildThread);
      return;
    }
  }
  if(!TerminateJobObject(m_JobObject, 1))
    exit(1);
}

/*
  Select all services from the list
*/
void CUpgradeDlg::OnBnSelectAll()
{
  for(int i=0; i < m_Services.GetCount(); i++)
   m_Services.SetCheck(i, 1);
  m_Ok.EnableWindow(TRUE);
}

/*
  Clear all services in the list
*/
void CUpgradeDlg::OnBnClearAll()
{
  for(int i=0; i < m_Services.GetCount(); i++)
   m_Services.SetCheck(i, 0);
  m_Ok.EnableWindow(FALSE);
}
