//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USERES("emb_sample.res");
USEFORM("emb_samples.cpp", Form1);
USELIB("libmysqld.lib");
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
        try
        {
                 Application->Initialize();
                 Application->Title = "MySQL Embedded Server Sample";
                 Application->CreateForm(__classid(TForm1), &Form1);
                 Application->Run();
        }
        catch (Exception &exception)
        {
                 Application->ShowException(&exception);
        }
        return 0;
}
//---------------------------------------------------------------------------
