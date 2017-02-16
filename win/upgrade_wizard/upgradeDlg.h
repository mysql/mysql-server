
// upgradeDlg.h : header file
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include <string>


// CUpgradeDlg dialog
class CUpgradeDlg : public CDialog
{
  // Construction
public:
  CUpgradeDlg(CWnd* pParent = NULL); // standard constructor

  // Dialog Data
  enum { IDD = IDD_UPGRADE_DIALOG };

protected:
  virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

  // job object for current process and children
  HANDLE m_JobObject;

  // Services are being upgraded
  BOOL m_UpgradeRunning;

  // ProgressBar related: number of services to upgrade
  int m_ProgressTotal;

  //ProgressBar related: current service being upgraded
  int m_ProgressCurrent; 

protected:
  HICON m_hIcon;

  // Generated message map functions
  virtual BOOL OnInitDialog();
  void PopulateServicesList();
  afx_msg void OnPaint();
  afx_msg HCURSOR OnQueryDragIcon();
  DECLARE_MESSAGE_MAP()
public:
  void SelectService(int index);
  void UpgradeServices();
  void UpgradeOneService(const std::string& name);
  void ErrorExit(const char *);
  std::string m_InstallDir;
  CCheckListBox m_Services;
  CProgressCtrl m_Progress;
  CButton m_Ok;
  CButton m_Cancel;
  CButton m_SelectAll;
  CButton m_ClearAll;
  int m_MajorVersion;
  int m_MinorVersion;
  int m_PatchVersion;

  CEdit m_IniFilePath;
  afx_msg void OnLbnSelchangeList1();
  afx_msg void OnChkChange();
  CEdit m_DataDir;
  CEdit m_Version;
  afx_msg void OnBnClickedOk();
  afx_msg void OnBnClickedCancel();
  afx_msg void OnBnSelectAll();
  afx_msg void OnBnClearAll();
  CEdit m_IniFileLabel;
  CEdit m_DataDirLabel;
  CEdit m_VersionLabel;
};
