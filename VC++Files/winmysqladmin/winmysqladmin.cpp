//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
HINSTANCE g_hinst;
USERES("winmysqladmin.res");
USEFORM("main.cpp", Form1);
USEFORM("initsetup.cpp", Form2);
USEFORM("db.cpp", dbfrm);
USELIB("lib\mysqlclient.lib");
USELIB("lib\myisammrg.lib");
USELIB("lib\heap.lib");
USELIB("lib\myisam.lib");
USELIB("lib\mysys.lib");
USELIB("lib\regex.lib");
USELIB("lib\strings.lib");
USELIB("lib\zlib.lib");
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
        try
        {
                 Application->Initialize();
                 Application->HelpFile = "C:\\mysql\\bin\\WINMYSQLADMIN.HLP";
                 Application->Title = "WinMySQLadmin 1.0";
                 Application->CreateForm(__classid(TForm1), &Form1);
                 Application->CreateForm(__classid(TForm2), &Form2);
                 Application->CreateForm(__classid(Tdbfrm), &dbfrm);
                 Application->Run();
        }
        catch (Exception &exception)
        {
                 Application->ShowException(&exception);
        }
        return 0;
}
//---------------------------------------------------------------------------
