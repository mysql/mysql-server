//---------------------------------------------------------------------------
#ifndef emb_samplesH
#define emb_samplesH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Grids.hpp>
#include <ImgList.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include <Buttons.hpp>
#include <deque.h>
//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
        TGroupBox *GroupBox1;
        TTreeView *DBView;
        TTreeView *TableView;
        TStringGrid *desc_table_grid;
        TImageList *ImageList2;
        TStatusBar *StatusBar1;
        TImage *Image1;
        TBitBtn *ToggleButton;
        TTimer *Timer1;
        TLabel *Label1;
        TEdit *info_server;
        TLabel *Label2;
        void __fastcall Timer1Timer(TObject *Sender);
        void __fastcall FormCreate(TObject *Sender);
        void __fastcall ToggleButtonClick(TObject *Sender);
        void __fastcall FormDestroy(TObject *Sender);
        void __fastcall DBViewClick(TObject *Sender);
        void __fastcall TableViewClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
        bool is_server_started;
        AnsiString db_root_caption;
        TTreeNode *db_root, *MySQLDbs, *tables_node, *tables_tree;
        void __fastcall computer_ip(void);
        bool __fastcall get_dbs(void);
        bool __fastcall get_tables(String db_name);
        bool __fastcall get_desc_table(String table_name);
        bool __fastcall connect_server();
        void __fastcall clean_desc_grid(void);
        void __fastcall titles_grid(void);
        void __fastcall fill_tree(deque<string> rows,
                                  TTreeNode *root,
                                  TTreeNode *child,
                                  TTreeView *View,
                                  int image_index);

        __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
