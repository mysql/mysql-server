//---------------------------------------------------------------------------
#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include <Grids.hpp>
#include <ImgList.hpp>
#include <Menus.hpp>
#include <Dialogs.hpp>
#include <string.h>

#define MYWM_NOTIFY         (WM_APP+100)
#define IDC_MYICON                     1006
extern HINSTANCE g_hinst;
LRESULT IconDrawItem(LPDRAWITEMSTRUCT lpdi);
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
        TStatusBar *StatusLine;
        TPanel *Panel1;
        TImage *Image1;
        TLabel *Label1;
        TLabel *Label2;
        TLabel *Label3;
        TLabel *Label8;
        TImage *Image3;
        TImage *Image2;
        TPageControl *PageControl1;
        TTabSheet *TabSheet1;
        TSpeedButton *SpeedButton1;
        TGroupBox *GroupBox1;
        TLabel *Label4;
        TLabel *Label5;
        TLabel *Label6;
        TLabel *Label14;
        TLabel *Label17;
        TEdit *Localhost;
        TEdit *Localuser;
        TEdit *OS;
        TMemo *Memo2;
        TEdit *Edit2;
        TGroupBox *GroupBox2;
        TMemo *Memo3;
        TGroupBox *GroupBox3;
        TLabel *Label13;
        TLabel *Label15;
        TLabel *Label16;
        TLabel *Label7;
        TLabel *Label47;
        TLabel *Label44;
        TLabel *Label42;
        TLabel *Label45;
        TEdit *Edit3;
        TEdit *Edit4;
        TEdit *Edit5;
        TEdit *Edit6;
        TEdit *st29;
        TEdit *st27;
        TEdit *st25;
        TEdit *st28;
        TTabSheet *TabSheet2;
        TTabSheet *TabSheet3;
        TLabel *Label18;
        TSpeedButton *SpeedButton2;
        TEdit *BaseDir;
        TMemo *Memo1;
        TRadioGroup *RadioGroup1;
        TRadioButton *ShareVer;
        TRadioButton *MysqldVer;
        TRadioButton *OptVer;
        TRadioButton *NtVer;
        TButton *Button2;
        TButton *Button3;
        TButton *Button1;
        TTabSheet *TabSheet4;
        TMemo *Memo4;
        TButton *Button5;
        TTabSheet *TabSheet5;
        TStringGrid *StringGrid1;
        TButton *Button11;
        TTabSheet *TabSheet6;
        TStringGrid *StringGrid2;
        TButton *Button10;
        TPopupMenu *PopupMenu1;
        TMenuItem *Showme1;
        TMenuItem *N1;
        TMenuItem *Win9;
        TMenuItem *Swin9;
        TMenuItem *N3;
        TMenuItem *SSW9;
        TMenuItem *N4;
        TMenuItem *ShutDownBoth1;
        TMenuItem *N2;
        TMenuItem *WinNT;
        TMenuItem *ShutDownthisTool1;
        TMenuItem *N5;
        TMenuItem *StopS;
        TMenuItem *N6;
        TMenuItem *RService;
        TMenuItem *N7;
        TMenuItem *Standa;
        TImageList *ImageList1;
        TTimer *Timer1;
        TTimer *Timer2;
        TTimer *Timer3;
        TSpeedButton *SpeedButton3;
        TSpeedButton *Extended;
        TLabel *Label9;
        TEdit *st26;
        TLabel *Label43;
        TEdit *st24;
        TLabel *Label41;
        TEdit *st23;
        TLabel *Label40;
        TEdit *st22;
        TLabel *Label39;
        TTabSheet *TabSheet8;
        TSaveDialog *SaveFileDialog;
        TPrinterSetupDialog *PrinterSetupDialog1;
        TPrintDialog *PrintDialog1;
        TRichEdit *Memo5;
        TGroupBox *GroupBox5;
        TSpeedButton *SpeedButton4;
        TSpeedButton *SpeedButton5;
        TSpeedButton *SpeedButton7;
        TSpeedButton *SpeedButton6;
        TGroupBox *GroupBox6;
        TSpeedButton *SpeedButton8;
        TSpeedButton *SpeedButton9;
        TSpeedButton *SpeedButton10;
        TSpeedButton *SpeedButton11;
        TSpeedButton *SpeedButton12;
        TTabSheet *TabSheet9;
        TImageList *ImageList2;
        TPopupMenu *PopupMenu2;
        TMenuItem *CreateDatabaseS;
        TMenuItem *DeleteDatabaseS;
        TMenuItem *RefreshS;
        TMenuItem *N8;
        TMenuItem *N9;
        TMenuItem *N10;
        TGroupBox *GroupBox7;
        TTreeView *DBView;
        TGroupBox *GroupBox8;
        TTreeView *TableView;
        TGroupBox *GroupBox9;
        TStringGrid *StringGrid4;
        TMenuItem *FlushHosts1;
        TMenuItem *N11;
        TMenuItem *FlushLogs1;
        TMenuItem *N12;
        TMenuItem *FlushTables1;
        TGroupBox *GroupBox10;
        TStringGrid *StringGrid3;
        TImage *Image5;
        TStringGrid *StringGrid5;
        TSpeedButton *SpeedButton13;
        TPopupMenu *PopupMenu4;
        TMenuItem *KillProcess1;
        TMenuItem *N13;
        TMenuItem *FlushThreads1;
        void __fastcall FormCreate(TObject *Sender);
        void __fastcall Showme1Click(TObject *Sender);
        void __fastcall Timer1Timer(TObject *Sender);
        void __fastcall SpeedButton1Click(TObject *Sender);
        void __fastcall Timer2Timer(TObject *Sender);
        void __fastcall Swin9Click(TObject *Sender);
        void __fastcall SSW9Click(TObject *Sender);
        void __fastcall ShutDownBoth1Click(TObject *Sender);
        void __fastcall ShutDownthisTool1Click(TObject *Sender);
        void __fastcall StopSClick(TObject *Sender);
        void __fastcall RServiceClick(TObject *Sender);
        void __fastcall StandaClick(TObject *Sender);
        void __fastcall Button5Click(TObject *Sender);
        void __fastcall Timer3Timer(TObject *Sender);
        void __fastcall Button11Click(TObject *Sender);
        void __fastcall Button10Click(TObject *Sender);
        void __fastcall Button6Click(TObject *Sender);
        void __fastcall Button7Click(TObject *Sender);
        void __fastcall Button8Click(TObject *Sender);

        void __fastcall Button2Click(TObject *Sender);
        void __fastcall Button3Click(TObject *Sender);
        void __fastcall Button1Click(TObject *Sender);
        void __fastcall SpeedButton2Click(TObject *Sender);
        void __fastcall Button4Click(TObject *Sender);
        void __fastcall SpeedButton3Click(TObject *Sender);
        void __fastcall ExtendedClick(TObject *Sender);
        void __fastcall SpeedButton4Click(TObject *Sender);
        void __fastcall SpeedButton5Click(TObject *Sender);
        void __fastcall SpeedButton6Click(TObject *Sender);
        void __fastcall SpeedButton7Click(TObject *Sender);
        void __fastcall SpeedButton8Click(TObject *Sender);
        void __fastcall SpeedButton9Click(TObject *Sender);
        void __fastcall SpeedButton10Click(TObject *Sender);
        void __fastcall SpeedButton11Click(TObject *Sender);
        void __fastcall SpeedButton12Click(TObject *Sender);
        void __fastcall DeleteDatabaseSClick(TObject *Sender);
        void __fastcall DBViewClick(TObject *Sender);
        void __fastcall TableViewClick(TObject *Sender);
        void __fastcall TableViewChange(TObject *Sender, TTreeNode *Node);
        void __fastcall DBViewChange(TObject *Sender, TTreeNode *Node);
      
        void __fastcall RefreshSClick(TObject *Sender);
        void __fastcall CreateDatabaseSClick(TObject *Sender);
        void __fastcall FlushHosts1Click(TObject *Sender);
        void __fastcall FlushLogs1Click(TObject *Sender);
        void __fastcall FlushTables1Click(TObject *Sender);
        void __fastcall SpeedButton13Click(TObject *Sender);
        void __fastcall KillProcess1Click(TObject *Sender);
        void __fastcall FlushThreads1Click(TObject *Sender);



       


        
private:	// User declarations
        void __fastcall DrawItem(TMessage& Msg);
        void __fastcall MyNotify(TMessage& Msg);
        bool __fastcall TrayMessage(DWORD dwMessage);
        HANDLE __fastcall IconHandle(void);
        void __fastcall ToggleState(void);
        PSTR __fastcall TipText(void);
        void __fastcall WMQueryEndSession(TWMQueryEndSession &msg);
        AnsiString __fastcall TheComputer();
        AnsiString __fastcall TheUser();
        AnsiString __fastcall TheOS();
        void __fastcall TakeIP(void);
        void __fastcall GetmemStatus(void);
        void __fastcall ShowHelp(void);
        void __fastcall ContinueLoad(void);
        void __fastcall MyODBC(void);
        void __fastcall IsMyIniUp(void);
        void __fastcall QuickSearch(void);
        AnsiString __fastcall TheWinDir();
        void __fastcall FillMyIni(void);
        void __fastcall GetBaseDir(void);
        bool __fastcall MySQLSignal();
        bool __fastcall mysqldstart();
        bool __fastcall SeekErrFile();
        AnsiString __fastcall TheDir();
        bool __fastcall TheServiceStart();
        bool __fastcall TheServicePause();
        bool __fastcall TheServiceResume();
        bool __fastcall TheServiceStatus();
        bool __fastcall TheServiceCreate();
        bool __fastcall TheServiceRemove();
        bool __fastcall Shutd();
        void __fastcall ClearBox(void);
        bool __fastcall TheServerPath();
        void __fastcall GetServerOptions(void);
        void __fastcall GetReportServer(void);


        TFileStream *MyFile;
        String FName;

        void __fastcall IsMySQLInit(void);
        void __fastcall GetServerStatus(void);
        bool __fastcall GetExtendedStatus();
        bool __fastcall GetProcess();
        bool __fastcall GetVariables();
        bool __fastcall nice_time(AnsiString buff);
        String  __fastcall GetString(String k);
        String  __fastcall GetNumberServer();
        // pointers for database screen
        TTreeNode *MySQLNode, *MySQLDbs, *MySQLNodeT,  *MySQLTbs;

        bool __fastcall GetMainRoot();
        bool __fastcall IsDatabase(String Name);
        bool __fastcall IsTable(String Name);
        void __fastcall CleanTree(void);
        void __fastcall CleanGrid(void);
        bool __fastcall IsIndex(String Name);
        void __fastcall CleanGridI(void);
        bool __fastcall KillPID();



public:		// User declarations
      __fastcall TForm1(TComponent* Owner);
      void __fastcall GetServerFile(void);
      void __fastcall CreateMyIniFile(void);
      bool __fastcall CreatingShortCut();
      bool __fastcall CreatingDB();
      void __fastcall OutRefresh(void);
      bool __fastcall CreatingTable(String TheTable);

      bool IsConnect ;

      

      STARTUPINFO si;
      PROCESS_INFORMATION pi;
      BEGIN_MESSAGE_MAP
      MESSAGE_HANDLER(WM_DRAWITEM,TMessage,DrawItem)
      MESSAGE_HANDLER(MYWM_NOTIFY,TMessage,MyNotify)
      MESSAGE_HANDLER(WM_QUERYENDSESSION,TWMQueryEndSession,WMQueryEndSession)
      END_MESSAGE_MAP(TForm)
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
