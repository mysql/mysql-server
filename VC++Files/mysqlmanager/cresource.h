#ifndef _CRESOURCE_H
#define _CRESOURCE_H

/////////////////////////////////////////////////////////////////////////////

#define MYSQL_PORT_AS_STRING "3306" /* Can't use # in preprocessor because of bugs in VC++ 5.0 */

class CResource
{
public:
   enum eRESOURCETYPE
   {
      eNone
   ,  eServer
   ,  eDatabase
   ,  eTable
   ,  eField
   ,  eProcesslist
   ,  eProcesslistItem
   };
   virtual LPCTSTR GetDisplayName() { return ""; }
   virtual LPCTSTR GetHostName() { return LOCAL_HOST; }
   virtual LPCTSTR GetUserName() { return "root"; }
   virtual LPCTSTR GetPassword() { return ""; }
   virtual LPCTSTR GetPortName() { return MYSQL_PORT_AS_STRING; }
   virtual int     GetPortNumber() { return MYSQL_PORT; }
   virtual eRESOURCETYPE GetType() { return eNone; }
};

/////////////////////////////////////////////////////////////////////////////

class CResourceServer : public CResource
{
public:
   CResourceServer(LPCTSTR pszName = "",LPCTSTR pszHost = LOCAL_HOST ,LPCTSTR pszUser = "root", LPCTSTR pszPassword = "", LPCTSTR pszPort = MYSQL_PORT_AS_STRING)
       : m_strName(pszName)
       , m_strHost(pszHost)
       , m_strUser(pszUser)
       , m_strPassword(pszPassword)
       , m_strPort(pszPort)
   {
   }
   virtual LPCTSTR GetDisplayName() { return m_strName; }
   virtual LPCTSTR GetHostName() { return m_strHost; }
   virtual LPCTSTR GetUserName() { return m_strUser; }
   virtual LPCTSTR GetPassword() { return m_strPassword; }
   virtual eRESOURCETYPE GetType() { return eServer; }
   virtual LPCTSTR GetPortName() { return m_strPort; }
   virtual int     GetPortNumber() { return atoi(m_strPort); }
   CString     m_strName;
   CString     m_strHost;
   CString     m_strUser;
   CString     m_strPassword;
   CString     m_strPort;
   CStringArray   m_rgFields;
};

/////////////////////////////////////////////////////////////////////////////

class CResourceDatabase : public CResource
{
public:
   CResourceDatabase(LPCTSTR pszName = "")
       : m_strName(pszName)
   {
   }
   virtual LPCTSTR GetDisplayName() { return m_strName; }
   virtual eRESOURCETYPE GetType() { return eDatabase; }
   CString     m_strName;
   CStringArray   m_rgFields;
};

/////////////////////////////////////////////////////////////////////////////

class CResourceTable : public CResource
{
public:
   CResourceTable(LPCTSTR pszName = "")
       : m_strName(pszName)
   {
   }
   virtual LPCTSTR GetDisplayName() { return m_strName; }
   virtual eRESOURCETYPE GetType() { return eTable; }
   CString     m_strName;
   CStringArray   m_rgFields;
};


/////////////////////////////////////////////////////////////////////////////

class CResourceField : public CResource
{
public:
   CResourceField(LPCTSTR pszName = "")
       : m_strName(pszName)
   {
   }
   virtual LPCTSTR GetDisplayName() { return m_strName; }
   virtual eRESOURCETYPE GetType() { return eField; }
   CString     m_strName;
   CStringArray   m_rgFields;
};



/////////////////////////////////////////////////////////////////////////////

class CResourceProcesslist : public CResource
{
public:
   CResourceProcesslist(LPCTSTR pszName = "Processlist")
       : m_strName(pszName)
   {
   }
   virtual LPCTSTR GetDisplayName() { return m_strName; }
   virtual eRESOURCETYPE GetType() { return eProcesslist; }
   CString        m_strName;
   CStringArray   m_rgFields;
};

/////////////////////////////////////////////////////////////////////////////

class CResourceProcesslistItem : public CResourceProcesslist
{
public:
   CResourceProcesslistItem(LPCTSTR pszName = "ProcesslistItem")
       : CResourceProcesslist(pszName)
   {
   }
   virtual eRESOURCETYPE GetType() { return eProcesslistItem; }
};


#endif
