//---------------------------------------------------------------------------
#ifndef dbH
#define dbH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class Tdbfrm : public TForm
{
__published:	// IDE-managed Components
        TImage *Image1;
        TLabel *Label1;
        TLabel *Label2;
        TEdit *Edit1;
        TSpeedButton *SpeedButton1;
        TSpeedButton *SpeedButton2;
        void __fastcall SpeedButton2Click(TObject *Sender);
        void __fastcall SpeedButton1Click(TObject *Sender);
private:	// User declarations
                bool __fastcall VerDBName();
public:		// User declarations
        __fastcall Tdbfrm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE Tdbfrm *dbfrm;
//---------------------------------------------------------------------------
#endif
