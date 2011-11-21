// Copyright (c) 2002, 2004, 2007 MySQL AB
// Use is subject to license terms.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; version 2 of the License.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "emb_samples.h"
#include <winsock2.h>
#include <mysql.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque.h>
bool b_new_line = false;
const char *server_groups[] = {
  "", "embedded", "server", NULL
};
MYSQL *MySQL;
deque<string> fill_rows(MYSQL_RES *res);
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
deque<string> fill_rows(MYSQL_RES *res)
{
 MYSQL_ROW row;
 deque<string> rows;

 while ((row=mysql_fetch_row(res)) != 0)
 {
   mysql_field_seek(res,0);
   for (unsigned int i=0 ; i < mysql_num_fields(res); i++)
    rows.push_back(row[i]);
 }
 return rows;
}
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TForm1::Timer1Timer(TObject *Sender)
{
 if (is_server_started)
 {
  ToggleButton->Caption = "Quit";
  Timer1->Enabled = false;
 }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FormCreate(TObject *Sender)
{
  is_server_started = false;
  computer_ip(); /* get the computer name and IP number */
  /* init the tree database screen */
  db_root = DBView->Items->Add(NULL, db_root_caption.UpperCase());
  db_root->ImageIndex = 0;
}
//---------------------------------------------------------------------------
/* button which handle the init of mysql server or quit the app */
void __fastcall TForm1::ToggleButtonClick(TObject *Sender)
{
 if (!is_server_started)
 {
  mysql_server_init(NULL, NULL, (char **)server_groups) ;
  connect_server();
  get_dbs();
 }
 else
 {
  mysql_server_end();
  Close();
 }
}
//---------------------------------------------------------------------------
void __fastcall TForm1::computer_ip(void)
{
  WORD wVersionRequested;
  WSADATA WSAData;
  wVersionRequested = MAKEWORD(1,1);
  WSAStartup(wVersionRequested,&WSAData);
  hostent *P;
  char s[128];
  in_addr in;
  char *P2;

  gethostname(s, 128);
  P = gethostbyname(s);
  db_root_caption = P->h_name;
  in.S_un.S_un_b.s_b1 = P->h_addr_list[0][0];
  in.S_un.S_un_b.s_b2 = P->h_addr_list[0][1];
  in.S_un.S_un_b.s_b3 = P->h_addr_list[0][2];
  in.S_un.S_un_b.s_b4 = P->h_addr_list[0][3];
  P2 = inet_ntoa(in);
  db_root_caption += " ( " + (AnsiString)P2 + " )";
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::connect_server()
{
  bool ret_value = false;

  MySQL = mysql_init(MySQL);
  if (!MySQL)
   return ret_value;
  if (mysql_real_connect(MySQL, NULL, NULL, NULL, NULL, 0, NULL, 0))
  {
   ret_value = true;
   is_server_started = true;
  }
  MySQL->reconnect= 1;
  return ret_value;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::FormDestroy(TObject *Sender)
{
  if (is_server_started)
   mysql_server_end();
}
//---------------------------------------------------------------------------

void __fastcall TForm1::DBViewClick(TObject *Sender)
{
  if (DBView->Selected != db_root && DBView->Selected != NULL)
  {
   get_tables(DBView->Selected->Text);
   clean_desc_grid();
  }
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::get_tables(String db_name)
{
  MYSQL_RES *res;
  AnsiString s_cmd;

  TableView->Items->Clear();
  s_cmd = "use ";
  s_cmd+= db_name.c_str();

  if (mysql_query(MySQL, s_cmd.c_str()) ||
      !(res=mysql_list_tables(MySQL,"%")))
   return false;

  tables_node = TableView->Items->Add(NULL, db_name.c_str());
  tables_node->ImageIndex = 1;
  tables_node->SelectedIndex = 1;

  deque<string> rows = fill_rows(res);

  mysql_free_result(res);
  fill_tree(rows,tables_tree,tables_node,TableView,2);

  return true;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::get_dbs(void)
{
 MYSQL_RES *res;

 if (!is_server_started)
  return false;

 if (!(res=mysql_list_dbs(MySQL,"%")))
  return false;

 deque<string> rows = fill_rows(res);

 mysql_free_result(res);
 fill_tree(rows,MySQLDbs,db_root,DBView,1);
 info_server->Text = mysql_get_server_info(MySQL);

 return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::fill_tree(deque<string> rows,
                                  TTreeNode *root,
                                  TTreeNode *child,
                                  TTreeView *View,
                                  int image_index)
{
  deque<string>::iterator r;
  for(r = rows.begin(); r != rows.end() ; r++)
  {
   root = View->Items->AddChild(child, (*r).c_str());
   root->ImageIndex = image_index;
   root->SelectedIndex = image_index;
  }
  child->Expanded = true;
}
//---------------------------------------------------------------------------
bool __fastcall TForm1::get_desc_table(String table_name)
{
 MYSQL_RES *res, *res1;
 MYSQL_ROW row;
 AnsiString use_db, show_cols, show_desc;
 unsigned int num_fields;
 int fields_control = 0, grid_row = 1, fields_number;
 b_new_line= true;

 clean_desc_grid();
 use_db = "use ";
 use_db+= DBView->Selected->Text.c_str();
 show_desc = "desc ";
 show_cols = "show full columns from ";
 show_cols+= table_name.c_str();
 show_desc+= table_name.c_str();

 if (mysql_query(MySQL, use_db.c_str() ))
  return false;

 if (mysql_query(MySQL, show_cols.c_str() ) ||
      !(res1=mysql_store_result(MySQL)))
 {
  if (mysql_query(MySQL, show_desc.c_str() ) ||
       !(res1=mysql_store_result(MySQL)))
   return false ;
 }
  mysql_fetch_row(res1);
  mysql_field_seek(res1,0);
  fields_number = (mysql_num_fields(res1) - 2);
  mysql_free_result(res1);

 if (mysql_query(MySQL, show_cols.c_str() ) ||
 !(res=mysql_store_result(MySQL)))
 {
  if (mysql_query(MySQL, show_desc.c_str() ) ||
      !(res=mysql_store_result(MySQL)))
   return false ;
 }
 titles_grid();
 while ((row=mysql_fetch_row(res)) != 0)
 {
   mysql_field_seek(res,0);
   for (num_fields=0 ; num_fields < mysql_num_fields(res); num_fields++)
   {
    if (fields_control <= fields_number )
    {
     desc_table_grid->Cells[fields_control][grid_row] = row[num_fields];
     fields_control++;
    }
    else
    {
     desc_table_grid->Cells[(fields_control)][grid_row] = row[num_fields];
     fields_control = 0;
     grid_row++ ;
     desc_table_grid->RowCount++;
    }
   }
 }
 desc_table_grid->RowCount--;
 mysql_free_result(res);
 return true;
}
//---------------------------------------------------------------------------
void __fastcall TForm1::TableViewClick(TObject *Sender)
{
 if (DBView->Selected != db_root && DBView->Selected != NULL)
  if (DBView->Selected != tables_tree)
   get_desc_table(TableView->Selected->Text);
}
//---------------------------------------------------------------------------
void __fastcall TForm1::clean_desc_grid(void)
{
  desc_table_grid->RowCount= 2;
  desc_table_grid->Cells[0][1] = "";
  desc_table_grid->Cells[1][1] = "";
  desc_table_grid->Cells[2][1] = "";
  desc_table_grid->Cells[3][1] = "";
  desc_table_grid->Cells[4][1] = "";
  desc_table_grid->Cells[5][1]  = "";
}
//---------------------------------------------------------------------------
void __fastcall TForm1::titles_grid(void)
{
 desc_table_grid->Cells[0][0] = "Field";
 desc_table_grid->Cells[1][0] = "Type";
 desc_table_grid->Cells[2][0] = "Null";
 desc_table_grid->Cells[3][0] = "Key";
 desc_table_grid->Cells[4][0] = "Default";
 desc_table_grid->Cells[5][0] = "Extra";
 desc_table_grid->Cells[6][0] = "Privileges";
}

