//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "db.h"
#include "main.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
Tdbfrm *dbfrm;
//---------------------------------------------------------------------------
__fastcall Tdbfrm::Tdbfrm(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall Tdbfrm::SpeedButton2Click(TObject *Sender)
{
  Close();
}
//---------------------------------------------------------------------------
void __fastcall Tdbfrm::SpeedButton1Click(TObject *Sender)
{
 if (VerDBName())
  {
    if (!Form1->CreatingDB())
     {
      Form1->OutRefresh();
      Edit1->Text = "";
      Application->MessageBox("The database was created", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
     } 
  }





}
//---------------------------------------------------------------------------
bool __fastcall Tdbfrm::VerDBName()
{
  String temp = Edit1->Text;
 if (Edit1->Text.IsEmpty())
  {
   Application->MessageBox("The name of the Database is Empty", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
   return false;
  }

 if (temp.Length() > 64)
  {
   Application->MessageBox("The name of the Database can't have more than 64 characters ", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
   return false;
  }

 for (int j = 1; j <= temp.Length(); j++)
  {
    if (temp[j] == ' ')
     {
      Application->MessageBox("The name of the Database can't have blank spaces ", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
      return false;
     }
    else if (temp[j] == '/')
     {
      Application->MessageBox("The name of the Database can't have frontslash (/)", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
      return false;
     }
    else if (temp[j] == '\\')
     {
      Application->MessageBox("The name of the Database can't have backslash (\\)", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
      return false;
     }
    else if (temp[j] == '.')
     {
      Application->MessageBox("The name of the Database can't have periods", "WinMySQLadmin 1.0", MB_OK |MB_ICONINFORMATION);
      return false;
     }
  }
 return true;
}
//---------------------------------------------------------------------------
