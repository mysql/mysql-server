//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "initsetup.h"
#include "main.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm2 *Form2;
//---------------------------------------------------------------------------
__fastcall TForm2::TForm2(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TForm2::BitBtn1Click(TObject *Sender)
{
 if ((Edit1->Text).IsEmpty() || (Edit2->Text).IsEmpty())
  Application->MessageBox("Fill the User name and Password text boxs ", "Winmysqladmin 1.0", MB_OK |MB_ICONINFORMATION);
 else
  {
    Form1->GetServerFile();
    Form1->CreateMyIniFile();
    Form1->CreatingShortCut();

    Close();
  }        
}
//---------------------------------------------------------------------------
void __fastcall TForm2::BitBtn2Click(TObject *Sender)
{
 Close();        
}
//---------------------------------------------------------------------------
void __fastcall TForm2::SpeedButton1Click(TObject *Sender)
{
 Application->HelpCommand(HELP_FINDER,0);              
}
//---------------------------------------------------------------------------
