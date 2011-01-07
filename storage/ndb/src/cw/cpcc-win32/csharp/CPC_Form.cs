/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using System.Threading;

namespace NDB_CPC
{
	/// <summary>
	/// Summary description for Form1.
	/// </summary>
	public class CPC : System.Windows.Forms.Form
	{
		private System.Windows.Forms.TreeView tvComputerCluster;
		private System.Windows.Forms.ContextMenu ctxTreeViewMenu;
		private System.Windows.Forms.ColumnHeader chComputer;
		private System.Windows.Forms.ColumnHeader chProcessName;
		private System.Windows.Forms.ContextMenu ctxListViewMenu;
		private System.Windows.Forms.MenuItem mainMenuItem;
		private System.Windows.Forms.ColumnHeader chProcesses;
		private System.Windows.Forms.MainMenu mainMenu;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.MenuItem menuItem7;
		private System.Windows.Forms.MenuItem menuItem10;
		private System.Windows.Forms.MenuItem mainMenuFile;
		private System.Windows.Forms.MenuItem mainMenuComputer;
		private System.Windows.Forms.MenuItem subMenuComputerAdd;
		private System.Windows.Forms.MenuItem subMenuComputerRemove;
		private System.Windows.Forms.MenuItem subMenuComputerDisconnect;
		private System.Windows.Forms.MenuItem subMenuComputerProperties;
		private System.ComponentModel.IContainer components;
		
		private System.Windows.Forms.MenuItem menuItem3;
		private System.Windows.Forms.MenuItem computerMenuAdd;
		private System.Windows.Forms.MenuItem computerMenuRemove;
		private System.Windows.Forms.MenuItem menuItem5;
		private System.Windows.Forms.MenuItem computerMenuDisconnect;
		private System.Windows.Forms.MenuItem computerMenuConnect;
		private System.Windows.Forms.MenuItem computerMenuProperties;
		private System.Windows.Forms.MenuItem menuItem11;
		private System.Windows.Forms.MenuItem tvCtxMenuComputerAdd;
		private System.Windows.Forms.MenuItem tvCtxMenuComputerRemove;
		private System.Windows.Forms.MenuItem tvCtxMenuComputerConnect;
		private System.Windows.Forms.MenuItem tvCtxMenuComputerDisconnect;
		private System.Windows.Forms.MenuItem tvCtxMenuComputerDefine;
		private System.Windows.Forms.MenuItem tvCtxMenuDatabaseNew;
		private System.Windows.Forms.MenuItem menuItem1;
		private System.Windows.Forms.MenuItem menuItem2;
		private System.Windows.Forms.MenuItem mainMenuDatabase;
		private System.Windows.Forms.MenuItem subMenuDatabaseCreate;
		private System.Windows.Forms.MenuItem menuItem8;
		private System.Windows.Forms.MenuItem tvCtxMenuProperties;
		private System.Windows.Forms.ImageList imageTV;

		private ComputerMgmt computerMgmt;
		private System.Windows.Forms.MenuItem computerMenuRefresh;
		private System.Windows.Forms.ListView listView;
		private System.Windows.Forms.ColumnHeader chComputerIP;
		private System.Windows.Forms.ColumnHeader chDatabase;
		private System.Windows.Forms.ColumnHeader chName;
		private System.Windows.Forms.ColumnHeader chOwner;
		private System.Windows.Forms.ColumnHeader chStatus;
		private System.Windows.Forms.Splitter splitter2;
		private System.Windows.Forms.Splitter splitterVertical;
		private System.Windows.Forms.Splitter splitterHorizont;
		private Thread guiThread;
		private float resizeWidthRatio;
		private System.Windows.Forms.MenuItem menuItem6;
		private System.Windows.Forms.MenuItem menuGetStatus;
		private System.Windows.Forms.MenuItem menuStartProcess;
		private System.Windows.Forms.MenuItem menuRestartProcess;
		private System.Windows.Forms.MenuItem menuStopProcess;
		private System.Windows.Forms.MenuItem menuRemoveProcess;
		private System.Windows.Forms.MenuItem menuRefresh;
		private System.Windows.Forms.OpenFileDialog openHostFileDialog;
		private System.Windows.Forms.SaveFileDialog saveHostFileDialog;
		private float resizeHeightRatio;
		private System.Windows.Forms.TextBox mgmConsole;
		int i;
		public CPC()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();
		
			// TODO: Add any constructor code after InitializeComponent call
			//
			computerMgmt = new ComputerMgmt();
			guiThread = new Thread(new ThreadStart(updateGuiThread));
			
	//		guiThread.Start();
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if (components != null) 
				{
					components.Dispose();
				}
			}
			//guiThread.Abort();
			base.Dispose( disposing );
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(CPC));
			this.tvComputerCluster = new System.Windows.Forms.TreeView();
			this.ctxTreeViewMenu = new System.Windows.Forms.ContextMenu();
			this.tvCtxMenuComputerAdd = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuComputerRemove = new System.Windows.Forms.MenuItem();
			this.menuGetStatus = new System.Windows.Forms.MenuItem();
			this.menuItem6 = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuComputerConnect = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuComputerDisconnect = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuDatabaseNew = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuComputerDefine = new System.Windows.Forms.MenuItem();
			this.menuItem8 = new System.Windows.Forms.MenuItem();
			this.tvCtxMenuProperties = new System.Windows.Forms.MenuItem();
			this.imageTV = new System.Windows.Forms.ImageList(this.components);
			this.ctxListViewMenu = new System.Windows.Forms.ContextMenu();
			this.menuStartProcess = new System.Windows.Forms.MenuItem();
			this.menuRestartProcess = new System.Windows.Forms.MenuItem();
			this.menuStopProcess = new System.Windows.Forms.MenuItem();
			this.menuRemoveProcess = new System.Windows.Forms.MenuItem();
			this.menuRefresh = new System.Windows.Forms.MenuItem();
			this.computerMenuAdd = new System.Windows.Forms.MenuItem();
			this.menuItem3 = new System.Windows.Forms.MenuItem();
			this.computerMenuRemove = new System.Windows.Forms.MenuItem();
			this.menuItem5 = new System.Windows.Forms.MenuItem();
			this.computerMenuDisconnect = new System.Windows.Forms.MenuItem();
			this.computerMenuConnect = new System.Windows.Forms.MenuItem();
			this.menuItem11 = new System.Windows.Forms.MenuItem();
			this.computerMenuProperties = new System.Windows.Forms.MenuItem();
			this.computerMenuRefresh = new System.Windows.Forms.MenuItem();
			this.chComputer = new System.Windows.Forms.ColumnHeader();
			this.chProcessName = new System.Windows.Forms.ColumnHeader();
			this.mainMenuItem = new System.Windows.Forms.MenuItem();
			this.chProcesses = new System.Windows.Forms.ColumnHeader();
			this.mainMenu = new System.Windows.Forms.MainMenu();
			this.mainMenuFile = new System.Windows.Forms.MenuItem();
			this.menuItem2 = new System.Windows.Forms.MenuItem();
			this.menuItem1 = new System.Windows.Forms.MenuItem();
			this.mainMenuComputer = new System.Windows.Forms.MenuItem();
			this.subMenuComputerAdd = new System.Windows.Forms.MenuItem();
			this.menuItem7 = new System.Windows.Forms.MenuItem();
			this.subMenuComputerDisconnect = new System.Windows.Forms.MenuItem();
			this.subMenuComputerRemove = new System.Windows.Forms.MenuItem();
			this.menuItem10 = new System.Windows.Forms.MenuItem();
			this.subMenuComputerProperties = new System.Windows.Forms.MenuItem();
			this.mainMenuDatabase = new System.Windows.Forms.MenuItem();
			this.subMenuDatabaseCreate = new System.Windows.Forms.MenuItem();
			this.panel1 = new System.Windows.Forms.Panel();
			this.mgmConsole = new System.Windows.Forms.TextBox();
			this.splitterHorizont = new System.Windows.Forms.Splitter();
			this.splitter2 = new System.Windows.Forms.Splitter();
			this.listView = new System.Windows.Forms.ListView();
			this.chComputerIP = new System.Windows.Forms.ColumnHeader();
			this.chStatus = new System.Windows.Forms.ColumnHeader();
			this.chDatabase = new System.Windows.Forms.ColumnHeader();
			this.chName = new System.Windows.Forms.ColumnHeader();
			this.chOwner = new System.Windows.Forms.ColumnHeader();
			this.splitterVertical = new System.Windows.Forms.Splitter();
			this.openHostFileDialog = new System.Windows.Forms.OpenFileDialog();
			this.saveHostFileDialog = new System.Windows.Forms.SaveFileDialog();
			this.panel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// tvComputerCluster
			// 
			this.tvComputerCluster.CausesValidation = false;
			this.tvComputerCluster.ContextMenu = this.ctxTreeViewMenu;
			this.tvComputerCluster.Dock = System.Windows.Forms.DockStyle.Left;
			this.tvComputerCluster.ImageList = this.imageTV;
			this.tvComputerCluster.Name = "tvComputerCluster";
			this.tvComputerCluster.Nodes.AddRange(new System.Windows.Forms.TreeNode[] {
																						  new System.Windows.Forms.TreeNode("Computer", 0, 0),
																						  new System.Windows.Forms.TreeNode("Database", 5, 5)});
			this.tvComputerCluster.Size = new System.Drawing.Size(104, 333);
			this.tvComputerCluster.TabIndex = 5;
			this.tvComputerCluster.MouseDown += new System.Windows.Forms.MouseEventHandler(this.tvComputerCluster_MouseDown);
			this.tvComputerCluster.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.tvComputerCluster_AfterSelect);
			this.tvComputerCluster.BeforeCollapse += new System.Windows.Forms.TreeViewCancelEventHandler(this.tvComputerCluster_BeforeCollapse);
			this.tvComputerCluster.BeforeExpand += new System.Windows.Forms.TreeViewCancelEventHandler(this.tvComputerCluster_BeforeExpand);
			// 
			// ctxTreeViewMenu
			// 
			this.ctxTreeViewMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																							this.tvCtxMenuComputerAdd,
																							this.tvCtxMenuComputerRemove,
																							this.menuGetStatus,
																							this.menuItem6,
																							this.tvCtxMenuComputerConnect,
																							this.tvCtxMenuComputerDisconnect,
																							this.tvCtxMenuDatabaseNew,
																							this.tvCtxMenuComputerDefine,
																							this.menuItem8,
																							this.tvCtxMenuProperties});
			this.ctxTreeViewMenu.Popup += new System.EventHandler(this.ctxTreeViewMenu_Popup);
			// 
			// tvCtxMenuComputerAdd
			// 
			this.tvCtxMenuComputerAdd.Index = 0;
			this.tvCtxMenuComputerAdd.Text = "Add computer";
			this.tvCtxMenuComputerAdd.Click += new System.EventHandler(this.computerMenuAdd_Click);
			// 
			// tvCtxMenuComputerRemove
			// 
			this.tvCtxMenuComputerRemove.Index = 1;
			this.tvCtxMenuComputerRemove.Text = "Remove computer";
			this.tvCtxMenuComputerRemove.Click += new System.EventHandler(this.computerMenuRemove_Click);
			// 
			// menuGetStatus
			// 
			this.menuGetStatus.Index = 2;
			this.menuGetStatus.Text = "Get Status";
			this.menuGetStatus.Click += new System.EventHandler(this.menuGetStatus_Click);
			// 
			// menuItem6
			// 
			this.menuItem6.Index = 3;
			this.menuItem6.Text = "-";
			// 
			// tvCtxMenuComputerConnect
			// 
			this.tvCtxMenuComputerConnect.Index = 4;
			this.tvCtxMenuComputerConnect.Text = "Connect";
			// 
			// tvCtxMenuComputerDisconnect
			// 
			this.tvCtxMenuComputerDisconnect.Index = 5;
			this.tvCtxMenuComputerDisconnect.Text = "Disconnect";
			// 
			// tvCtxMenuDatabaseNew
			// 
			this.tvCtxMenuDatabaseNew.Index = 6;
			this.tvCtxMenuDatabaseNew.Text = "Create database...";
			this.tvCtxMenuDatabaseNew.Click += new System.EventHandler(this.subMenuDatabaseCreate_Click);
			// 
			// tvCtxMenuComputerDefine
			// 
			this.tvCtxMenuComputerDefine.Index = 7;
			this.tvCtxMenuComputerDefine.Text = "Define process...";
			this.tvCtxMenuComputerDefine.Click += new System.EventHandler(this.tvCtxMenuComputerDefine_Click);
			// 
			// menuItem8
			// 
			this.menuItem8.Index = 8;
			this.menuItem8.Text = "-";
			// 
			// tvCtxMenuProperties
			// 
			this.tvCtxMenuProperties.Index = 9;
			this.tvCtxMenuProperties.Text = "Properties";
			// 
			// imageTV
			// 
			this.imageTV.ColorDepth = System.Windows.Forms.ColorDepth.Depth8Bit;
			this.imageTV.ImageSize = new System.Drawing.Size(16, 16);
			this.imageTV.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageTV.ImageStream")));
			this.imageTV.TransparentColor = System.Drawing.Color.Transparent;
			// 
			// ctxListViewMenu
			// 
			this.ctxListViewMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																							this.menuStartProcess,
																							this.menuRestartProcess,
																							this.menuStopProcess,
																							this.menuRemoveProcess,
																							this.menuRefresh});
			this.ctxListViewMenu.Popup += new System.EventHandler(this.ctxListViewMenu_Popup);
			// 
			// menuStartProcess
			// 
			this.menuStartProcess.Index = 0;
			this.menuStartProcess.Text = "Start process";
			this.menuStartProcess.Click += new System.EventHandler(this.startProcess);
			// 
			// menuRestartProcess
			// 
			this.menuRestartProcess.Index = 1;
			this.menuRestartProcess.Text = "Restart process";
			this.menuRestartProcess.Click += new System.EventHandler(this.restartProcess);
			// 
			// menuStopProcess
			// 
			this.menuStopProcess.Index = 2;
			this.menuStopProcess.Text = "Stop process";
			this.menuStopProcess.Click += new System.EventHandler(this.stopProcess);
			// 
			// menuRemoveProcess
			// 
			this.menuRemoveProcess.Index = 3;
			this.menuRemoveProcess.Text = "Remove process";
			this.menuRemoveProcess.Click += new System.EventHandler(this.removeProcess);
			// 
			// menuRefresh
			// 
			this.menuRefresh.Index = 4;
			this.menuRefresh.Text = "Refresh";
			this.menuRefresh.Click += new System.EventHandler(this.menuRefresh_Click);
			// 
			// computerMenuAdd
			// 
			this.computerMenuAdd.Index = -1;
			this.computerMenuAdd.Text = "Add";
			this.computerMenuAdd.Click += new System.EventHandler(this.computerMenuAdd_Click);
			// 
			// menuItem3
			// 
			this.menuItem3.Index = -1;
			this.menuItem3.Text = "-";
			// 
			// computerMenuRemove
			// 
			this.computerMenuRemove.Index = -1;
			this.computerMenuRemove.Text = "Remove";
			this.computerMenuRemove.Click += new System.EventHandler(this.computerMenuRemove_Click);
			// 
			// menuItem5
			// 
			this.menuItem5.Index = -1;
			this.menuItem5.Text = "-";
			// 
			// computerMenuDisconnect
			// 
			this.computerMenuDisconnect.Index = -1;
			this.computerMenuDisconnect.Text = "Disconnect";
			// 
			// computerMenuConnect
			// 
			this.computerMenuConnect.Index = -1;
			this.computerMenuConnect.Text = "Connect";
			// 
			// menuItem11
			// 
			this.menuItem11.Index = -1;
			this.menuItem11.Text = "-";
			// 
			// computerMenuProperties
			// 
			this.computerMenuProperties.Index = -1;
			this.computerMenuProperties.Text = "Properties";
			// 
			// computerMenuRefresh
			// 
			this.computerMenuRefresh.Index = -1;
			this.computerMenuRefresh.Text = "Refresh";
			this.computerMenuRefresh.Click += new System.EventHandler(this.computerMenuRefresh_Click);
			// 
			// chComputer
			// 
			this.chComputer.Text = "Computer";
			// 
			// chProcessName
			// 
			this.chProcessName.Text = "Name";
			// 
			// mainMenuItem
			// 
			this.mainMenuItem.Index = -1;
			this.mainMenuItem.Text = "File";
			// 
			// chProcesses
			// 
			this.chProcesses.Text = "Id";
			// 
			// mainMenu
			// 
			this.mainMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					 this.mainMenuFile,
																					 this.mainMenuComputer,
																					 this.mainMenuDatabase});
			// 
			// mainMenuFile
			// 
			this.mainMenuFile.Index = 0;
			this.mainMenuFile.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																						 this.menuItem2,
																						 this.menuItem1});
			this.mainMenuFile.Text = "&File";
			// 
			// menuItem2
			// 
			this.menuItem2.Index = 0;
			this.menuItem2.Text = "&Import...";
			this.menuItem2.Click += new System.EventHandler(this.importHostFile);
			// 
			// menuItem1
			// 
			this.menuItem1.Index = 1;
			this.menuItem1.Text = "&Export...";
			this.menuItem1.Click += new System.EventHandler(this.exportHostFile);
			// 
			// mainMenuComputer
			// 
			this.mainMenuComputer.Index = 1;
			this.mainMenuComputer.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																							 this.subMenuComputerAdd,
																							 this.menuItem7,
																							 this.subMenuComputerDisconnect,
																							 this.subMenuComputerRemove,
																							 this.menuItem10,
																							 this.subMenuComputerProperties});
			this.mainMenuComputer.Text = "&Computer";
			// 
			// subMenuComputerAdd
			// 
			this.subMenuComputerAdd.Index = 0;
			this.subMenuComputerAdd.Text = "&Add Computer";
			this.subMenuComputerAdd.Click += new System.EventHandler(this.computerMenuAdd_Click);
			// 
			// menuItem7
			// 
			this.menuItem7.Index = 1;
			this.menuItem7.Text = "-";
			// 
			// subMenuComputerDisconnect
			// 
			this.subMenuComputerDisconnect.Index = 2;
			this.subMenuComputerDisconnect.Text = "&Disconnect";
			// 
			// subMenuComputerRemove
			// 
			this.subMenuComputerRemove.Index = 3;
			this.subMenuComputerRemove.Text = "&Remove Computer";
			this.subMenuComputerRemove.Click += new System.EventHandler(this.computerMenuRemove_Click);
			// 
			// menuItem10
			// 
			this.menuItem10.Index = 4;
			this.menuItem10.Text = "-";
			// 
			// subMenuComputerProperties
			// 
			this.subMenuComputerProperties.Index = 5;
			this.subMenuComputerProperties.Text = "&Properties";
			// 
			// mainMenuDatabase
			// 
			this.mainMenuDatabase.Index = 2;
			this.mainMenuDatabase.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																							 this.subMenuDatabaseCreate});
			this.mainMenuDatabase.Text = "&Database";
			this.mainMenuDatabase.Click += new System.EventHandler(this.subMenuDatabaseCreate_Click);
			// 
			// subMenuDatabaseCreate
			// 
			this.subMenuDatabaseCreate.Index = 0;
			this.subMenuDatabaseCreate.Text = "&Create database...";
			this.subMenuDatabaseCreate.Click += new System.EventHandler(this.subMenuDatabaseCreate_Click);
			// 
			// panel1
			// 
			this.panel1.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.mgmConsole,
																				 this.splitterHorizont,
																				 this.splitter2,
																				 this.listView});
			this.panel1.Dock = System.Windows.Forms.DockStyle.Fill;
			this.panel1.Location = new System.Drawing.Point(104, 0);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(384, 333);
			this.panel1.TabIndex = 6;
			// 
			// mgmConsole
			// 
			this.mgmConsole.AccessibleRole = System.Windows.Forms.AccessibleRole.StaticText;
			this.mgmConsole.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.mgmConsole.Location = new System.Drawing.Point(0, 231);
			this.mgmConsole.Multiline = true;
			this.mgmConsole.Name = "mgmConsole";
			this.mgmConsole.Size = new System.Drawing.Size(384, 96);
			this.mgmConsole.TabIndex = 5;
			this.mgmConsole.Text = "textBox1";
			this.mgmConsole.TextChanged += new System.EventHandler(this.mgmConsole_TextChanged);
			this.mgmConsole.Enter += new System.EventHandler(this.mgmConsole_Enter);
			// 
			// splitterHorizont
			// 
			this.splitterHorizont.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.splitterHorizont.Location = new System.Drawing.Point(0, 327);
			this.splitterHorizont.MinExtra = 100;
			this.splitterHorizont.MinSize = 100;
			this.splitterHorizont.Name = "splitterHorizont";
			this.splitterHorizont.Size = new System.Drawing.Size(384, 3);
			this.splitterHorizont.TabIndex = 4;
			this.splitterHorizont.TabStop = false;
			// 
			// splitter2
			// 
			this.splitter2.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.splitter2.Location = new System.Drawing.Point(0, 330);
			this.splitter2.Name = "splitter2";
			this.splitter2.Size = new System.Drawing.Size(384, 3);
			this.splitter2.TabIndex = 2;
			this.splitter2.TabStop = false;
			// 
			// listView
			// 
			this.listView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
																					   this.chComputerIP,
																					   this.chStatus,
																					   this.chDatabase,
																					   this.chName,
																					   this.chOwner});
			this.listView.ContextMenu = this.ctxListViewMenu;
			this.listView.Dock = System.Windows.Forms.DockStyle.Fill;
			this.listView.FullRowSelect = true;
			this.listView.Name = "listView";
			this.listView.Size = new System.Drawing.Size(384, 333);
			this.listView.TabIndex = 0;
			this.listView.View = System.Windows.Forms.View.Details;
			this.listView.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.listView_ColumnClick_1);
			this.listView.SelectedIndexChanged += new System.EventHandler(this.listView_SelectedIndexChanged);
			// 
			// chComputerIP
			// 
			this.chComputerIP.Text = "IP Adress";
			// 
			// chStatus
			// 
			this.chStatus.Text = "Status";
			// 
			// chDatabase
			// 
			this.chDatabase.Text = "Database";
			// 
			// chName
			// 
			this.chName.Text = "Name";
			// 
			// chOwner
			// 
			this.chOwner.Text = "Owner";
			// 
			// splitterVertical
			// 
			this.splitterVertical.Location = new System.Drawing.Point(104, 0);
			this.splitterVertical.MinSize = 100;
			this.splitterVertical.Name = "splitterVertical";
			this.splitterVertical.Size = new System.Drawing.Size(3, 333);
			this.splitterVertical.TabIndex = 7;
			this.splitterVertical.TabStop = false;
			this.splitterVertical.SplitterMoved += new System.Windows.Forms.SplitterEventHandler(this.splitterVertical_SplitterMoved);
			// 
			// openHostFileDialog
			// 
			this.openHostFileDialog.DefaultExt = "cpc";
			this.openHostFileDialog.Filter = "CPCd configuration files (*.cpc)|*.cpc| All Files (*.*)|*.*";
			this.openHostFileDialog.Title = "Import a CPCd configuration file";
			this.openHostFileDialog.FileOk += new System.ComponentModel.CancelEventHandler(this.openHostFileDialog_FileOk);
			// 
			// saveHostFileDialog
			// 
			this.saveHostFileDialog.Filter = "CPCd configuration files (*.cpc)|*.cpc| All Files (*.*)|*.*";
			this.saveHostFileDialog.Title = "Export a CPCd configuration file";
			this.saveHostFileDialog.FileOk += new System.ComponentModel.CancelEventHandler(this.saveHostFileDialog_FileOk);
			// 
			// CPC
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(488, 333);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.splitterVertical,
																		  this.panel1,
																		  this.tvComputerCluster});
			this.Menu = this.mainMenu;
			this.Name = "CPC";
			this.Text = "CPC";
			this.Resize += new System.EventHandler(this.CPC_Resize);
			this.MouseDown += new System.Windows.Forms.MouseEventHandler(this.CPC_MouseDown);
			this.Closing += new System.ComponentModel.CancelEventHandler(this.CPC_Closing);
			this.Load += new System.EventHandler(this.CPC_Load);
			this.Activated += new System.EventHandler(this.CPC_Activated);
			this.Paint += new System.Windows.Forms.PaintEventHandler(this.CPC_Paint);
			this.panel1.ResumeLayout(false);
			this.ResumeLayout(false);

		}
		#endregion

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main() 
		{
			Application.Run(new CPC());

		}

		private void tvComputerCluster_AfterSelect(object sender, System.Windows.Forms.TreeViewEventArgs e)
		{
			if(e.Node.Text.ToString().Equals("Database")) 
			{
				updateListViews("Database");
			
				return;
			}
			if(e.Node.Text.ToString().Equals("Computer"))
			{				
				//updateListViews();
				
				updateListViews("Computer");
				return;
			}
			if(e.Node.Parent.Text.ToString().Equals("Database"))
			{
				//updateListViews();
				listView.Columns.Clear();
				listView.Columns.Add(this.chName);
				listView.Columns.Add(this.chDatabase);
				listView.Columns.Add(this.chStatus);
				listView.Columns.Add(this.chOwner);
				updateDatabaseView(e.Node.Text.ToString());
			}

			if(e.Node.Parent.Text=="Computer")
			{
				//updateListViews();
				
				Computer c=computerMgmt.getComputer(e.Node.Text.ToString());
				string [] processcols= new string[5];
				ArrayList processes;
				processes = c.getProcesses();
				listView.Items.Clear();
				listView.Columns.Clear();
				listView.Columns.Add(this.chComputer);
				listView.Columns.Add(this.chDatabase);
				listView.Columns.Add(this.chName);
				listView.Columns.Add(this.chStatus);
				listView.Columns.Add(this.chOwner);
				if(processes != null ) 
				{
					
					listView.BeginUpdate();
					foreach(Process p in processes) 
					{
						processcols[0]=p.getComputer().getName();
						processcols[1]=p.getDatabase();
						processcols[2]=p.getName();
						processcols[3]=p.getStatusString();
						processcols[4]=p.getOwner();
						ListViewItem lvp= new ListViewItem(processcols);
						listView.Items.Add(lvp);
					}
					
					listView.EndUpdate();
				}


				listView.Show();
			}
		
		}

		

		private void ctxTreeViewMenu_Popup(object sender, System.EventArgs e)
		{
				tvCtxMenuComputerAdd.Enabled=true;
				tvCtxMenuComputerRemove.Enabled=true;
				tvCtxMenuComputerConnect.Enabled=true;
				tvCtxMenuComputerDisconnect.Enabled=true;
				tvCtxMenuComputerDefine.Enabled=true;
				menuGetStatus.Enabled=true;	
				tvCtxMenuDatabaseNew.Enabled=true;
				tvCtxMenuComputerAdd.Visible=true;
				tvCtxMenuComputerRemove.Visible=true;
				tvCtxMenuComputerConnect.Visible=true;
				tvCtxMenuComputerDisconnect.Visible=true;
				tvCtxMenuComputerDefine.Visible=true;
				tvCtxMenuDatabaseNew.Visible=true;	
				tvCtxMenuProperties.Visible=true;
				menuGetStatus.Visible=true;

				if(tvComputerCluster.SelectedNode.Text.Equals("Computer"))
				{
					tvCtxMenuComputerAdd.Enabled=true;
					tvCtxMenuComputerRemove.Enabled=false;
					tvCtxMenuComputerConnect.Enabled=false;
					tvCtxMenuComputerDisconnect.Enabled=false;
					tvCtxMenuComputerDefine.Enabled=false;
					tvCtxMenuDatabaseNew.Visible=false;
					menuGetStatus.Visible=false;
					return;
				}
				
				if(tvComputerCluster.SelectedNode.Text.Equals("Database"))
				{
				//	ctxTreeViewMenu.MenuItems.Add(menuDatabaseItem1);
					tvCtxMenuComputerAdd.Visible=false;
					tvCtxMenuComputerRemove.Visible=false;
					tvCtxMenuComputerConnect.Visible=false;
					tvCtxMenuComputerDisconnect.Visible=false;
					tvCtxMenuComputerDefine.Visible=false;
					tvCtxMenuDatabaseNew.Visible=true;
					tvCtxMenuDatabaseNew.Enabled=true;
					menuGetStatus.Visible=false;
					menuItem6.Visible=false;
					return;
				}
				if(tvComputerCluster.SelectedNode.Parent.Text.Equals("Computer"))
				{
				
					Computer c= computerMgmt.getComputer(tvComputerCluster.SelectedNode.Text.ToString());
					if(c.getStatus().Equals(Computer.Status.Disconnected)) 
					{
						tvCtxMenuComputerConnect.Enabled=true;
						tvCtxMenuComputerDisconnect.Enabled=false;
					}
					else 
					{
						tvCtxMenuComputerDisconnect.Enabled=true;
						tvCtxMenuComputerConnect.Enabled=false;
					}
					
					tvCtxMenuComputerAdd.Enabled=false;
					tvCtxMenuComputerRemove.Enabled=true;
					menuGetStatus.Visible=false;

					tvCtxMenuComputerDefine.Enabled=true;	
					tvCtxMenuDatabaseNew.Visible=false;
					return;
				}
					
				if(tvComputerCluster.SelectedNode.Parent.Text.Equals("Database"))
				{
					tvCtxMenuComputerAdd.Enabled=true;
					tvCtxMenuComputerRemove.Enabled=false;
					tvCtxMenuComputerConnect.Enabled=false;
					tvCtxMenuComputerDisconnect.Enabled=false;
					tvCtxMenuComputerDefine.Enabled=false;
					tvCtxMenuDatabaseNew.Visible=true;
					menuGetStatus.Visible=true;
					return;
				}

			
		}


		private void listView_SelectedIndexChanged(object sender, System.EventArgs e)
		{
			//MessageBox.Show(listView.SelectedItems[0].Text);
		}

	
		private void tvComputerCluster_MouseDown(object sender, System.Windows.Forms.MouseEventArgs e)
		{ /*
			TreeNode node = tvComputerCluster.GetNodeAt(e.X,e.Y);
			if(node==null)
			{
				return;
			}
			tvComputerCluster.SelectedNode=node;
//			updateListViews();
			tvComputerCluster.SelectedNode.Expand();
			*/
		}


		private void subMenuComputerRemove_Click(object sender, System.EventArgs e)
		{
			//ComputerRemoveDialog crd=new ComputerRemoveDialog(computerMgmt);
			//crd.Show();
			//updateListViews();
/*			string computer =  tvComputerCluster.SelectedNode.Text.ToString();
			if(MessageBox.Show(this,"Are you sure you want to remove: " +computer+ "?","Remove computer",MessageBoxButtons.YesNo)==DialogResult.Yes)
			{
				computerMgmt.RemoveComputer(computer);	
			}
*/
		}

		private void subMenuComputerAdd_Click(object sender, System.EventArgs e)
		{
			ComputerAddDialog cad=new ComputerAddDialog(computerMgmt);
			cad.ShowDialog();
			cad.Dispose();
///			updateListViews(tvComputerCluster.SelectedNode.Text.ToString());
		}

		

		private void updateListViews(string node)
		{
			if(node.Equals("Computer"))
			{
				listView.Columns.Clear();
				listView.Items.Clear();
				ArrayList list= computerMgmt.getComputerCollection();
				string [] computercols= new string[2];
			
			
				listView.BeginUpdate();
				listView.Columns.Add(this.chComputer);
				listView.Columns.Add(this.chStatus);
				foreach (Computer computer in list) 
				{
					computercols[0]=computer.getName();
					computercols[1]=computer.getStatusString();
				
					ListViewItem lvc= new ListViewItem(computercols);
	
					listView.Items.Add(lvc);
			
				}
				listView.EndUpdate();
				listView.Show();
			}

			if(node.Equals("Database"))
			{
				
				ArrayList databases= computerMgmt.getDatabaseCollection();
				string [] dbcols= new string[3];
			
			
				listView.BeginUpdate();
				listView.Items.Clear();
				listView.Columns.Clear();
				listView.Columns.Add(this.chDatabase);
				listView.Columns.Add(this.chStatus);
				listView.Columns.Add(this.chOwner);
				foreach (Database db in databases) 
				{
					dbcols[0]=db.getName();
					dbcols[1]=db.getStatusString();
					dbcols[2]=db.getOwner();
				
					ListViewItem lvc= new ListViewItem(dbcols);
	
					listView.Items.Add(lvc);
			
				}
				listView.EndUpdate();
				
				listView.Show();
			}

		}

		public void updateDatabaseView(string database) 
		{
			Database d=computerMgmt.getDatabase(database);
			string [] processcols= new string[5];
			ArrayList processes = d.getProcesses();
			listView.Items.Clear();
			if(processes != null ) 
			{
					
				listView.BeginUpdate();
				listView.Columns.Clear();
				listView.Columns.Add(this.chComputer);
				listView.Columns.Add(this.chDatabase);
				listView.Columns.Add(this.chName);
				listView.Columns.Add(this.chStatus);
				listView.Columns.Add(this.chOwner);

				foreach(Process p in processes) 
				{
					processcols[0]=p.getComputer().getName();
					processcols[1]=p.getDatabase();
					processcols[2]=p.getName();
					processcols[3]=p.getStatusString();
					processcols[4]=p.getOwner();
					ListViewItem lvp= new ListViewItem(processcols);
					listView.Items.Add(lvp);
				}
					
				listView.EndUpdate();
			}

			listView.Show();
		}

		private void updateTreeViews()
		{
			//tvComputerCluster.Nodes.Clear();
			ArrayList computers= computerMgmt.getComputerCollection();

			ArrayList databases= computerMgmt.getDatabaseCollection();

			tvComputerCluster.BeginUpdate();
			tvComputerCluster.Nodes[0].Nodes.Clear();
			tvComputerCluster.Nodes[1].Nodes.Clear();
			if(computers != null) 
			{
				foreach (Computer computer in computers) 
				{	
					tvComputerCluster.Nodes[0].Nodes.Add(new TreeNode(computer.getName().ToString()));			
				}
			}
			if(databases != null) 
			{
				foreach (Database db in databases) 
				{	
					tvComputerCluster.Nodes[1].Nodes.Add(new TreeNode(db.getName().ToString()));			
				}
			}
			
			tvComputerCluster.EndUpdate();
		}


		private void CPC_MouseDown(object sender, System.Windows.Forms.MouseEventArgs e)
		{
			//updateListViews();
			//updateTreeViews();
			
		}

		private void CPC_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			if(tvComputerCluster.SelectedNode!=null) 
			{
				if(tvComputerCluster.SelectedNode.Text.ToString().Equals("Computer"))
					updateListViews("Computer");
			}
			
			//updateListViews();
			//updateTreeViews();
		}

		private void CPC_Activated(object sender, System.EventArgs e)
		{
			updateListViews(tvComputerCluster.SelectedNode.Text.ToString());
			//updateListViews();
			updateTreeViews();
		}


		private void computerMenuAdd_Click(object sender, System.EventArgs e)
		{
			ComputerAddDialog cad=new ComputerAddDialog(computerMgmt);
			cad.ShowDialog();
			cad.Dispose();
			
		}

		private void computerMenuRemove_Click(object sender, System.EventArgs e)
		{

			string computer =  tvComputerCluster.SelectedNode.Text.ToString();		
			if(MessageBox.Show("Are you sure you want to remove: " + computer +"?\n" + "This will remove all processes on the computer!" ,"Remove selected computer",MessageBoxButtons.YesNo, MessageBoxIcon.Question)== DialogResult.Yes)
			{
				removeComputer(computer);
			}
		}

		private void removeComputer(string computer)
		{
			ArrayList processes;
			Computer c=computerMgmt.getComputer(computer);
			processes = c.getProcesses();
			
			/*foreach(Process p in processes) 
			{
				removeProcess(computer,p.getName());
				processes=c.getProcesses();
			}
*/
			if(computerMgmt.RemoveComputer(computer)) 
			{
				tvComputerCluster.SelectedNode=tvComputerCluster.SelectedNode.PrevVisibleNode;
				this.updateTreeViews();
				this.updateListViews("Computer");

				if(tvComputerCluster.SelectedNode!=null)
					this.updateListViews(tvComputerCluster.SelectedNode.Text.ToString());
				//updateListViews();
			}
		}

		private void listView_ColumnClick(object sender, System.Windows.Forms.ColumnClickEventArgs e)
		{
			
			if(listView.Sorting.Equals(SortOrder.Ascending))
				listView.Sorting=SortOrder.Descending;
			else
				listView.Sorting=SortOrder.Ascending;
			
		}

		
		private void subMenuDatabaseCreate_Click(object sender, System.EventArgs e)
		{
			PanelWizard p = new PanelWizard(this.computerMgmt);
			p.ShowDialog();
		}

		private void tvCtxMenuComputerDefine_Click(object sender, System.EventArgs e)
		{
			ProcessDefineDialog pdd = new ProcessDefineDialog(this.computerMgmt, 
			tvComputerCluster.SelectedNode.Text.ToString());
			pdd.Show();
		}

		private void listView_ItemActivate(object sender, System.EventArgs e)
		{
			updateDatabaseView(listView.SelectedItems[0].Text.ToString());
			for(int i=0;i<tvComputerCluster.Nodes[1].Nodes.Count;i++) 
			{
				if(tvComputerCluster.Nodes[1].Nodes[i].Text.ToString().Equals(listView.SelectedItems[0].Text.ToString()))
				{
					tvComputerCluster.SelectedNode=tvComputerCluster.Nodes[1].Nodes[i];
					break;
				}
			}
			
			
		}

		private void CPC_Resize(object sender, System.EventArgs e)
		{
			if(this.Width < 200) this.Width=200;
			if(this.Height <200) this.Height=200;
			this.tvComputerCluster.Width=(int)(this.Width*this.resizeWidthRatio);
			this.listView.Height=(int)(this.Height*this.resizeHeightRatio);

			//this.Size=new System.Drawing.Size((int)(this.Size.Width*this.tvComputerCluster.Width
			
		}

		

		private void updateGuiThread()
		{
			while(true) {
				if(tvComputerCluster.SelectedNode!=null) 
				{
					if(tvComputerCluster.SelectedNode.Text.ToString().Equals("Computer"))
						updateListViews("Computer");
				}
				Thread.Sleep(1000);
			}
		}

		private void computerMenuRefresh_Click(object sender, System.EventArgs e)
		{
			updateListViews("Computer");
		}

		private void CPC_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			/*clean up*/
			ArrayList comp = this.computerMgmt.getComputerCollection();
			foreach(Computer c in comp)
			{
				c.disconnect();
			}
		}

		private void CPC_Load(object sender, System.EventArgs e)
		{
			this.tvComputerCluster.Width=104;
			resizeWidthRatio =(float) ((float)(this.tvComputerCluster.Width)/(float)(this.Width));
			resizeHeightRatio = (float) ((float)(this.listView.Height)/(float)(this.Height));
			listView.Columns.Clear();
			listView.Columns.Add(this.chComputer);
			listView.Columns.Add(this.chStatus);
			
		}

		private void splitterVertical_SplitterMoved(object sender, System.Windows.Forms.SplitterEventArgs e)
		{
			if(this.Width < 500)
				this.Width=500;
		}

		private void menuGetStatus_Click(object sender, System.EventArgs e)
		{
			
		}

		private void tvComputerCluster_BeforeExpand(object sender, System.Windows.Forms.TreeViewCancelEventArgs e)
		{
			if (e.Node.Parent!=null && e.Node.Nodes.Count !=0)
				e.Cancel=true;
			if(e.Node.IsExpanded)
				e.Cancel=true;
		}

		private void tvComputerCluster_BeforeCollapse(object sender, System.Windows.Forms.TreeViewCancelEventArgs e)
		{
			e.Cancel=true;
			if (e.Node.Parent!=null && e.Node.Nodes.Count !=0)
				e.Cancel=true;
			if(e.Node.IsExpanded)
				e.Cancel=false;
		}

	

		private void ctxListViewMenu_Popup(object sender, System.EventArgs e)
		{
			
			menuStartProcess.Visible=false;
			menuStopProcess.Visible=false;
			menuRestartProcess.Visible=false;
			menuRemoveProcess.Visible=false;
			menuRefresh.Visible=false;
			

			if(this.tvComputerCluster.SelectedNode.Text.Equals("Computer"))
			{
				return;
			}

			if(this.tvComputerCluster.SelectedNode.Text.Equals("Database"))
			{
				return;
			}

			if(this.tvComputerCluster.SelectedNode.Parent.Text.Equals("Computer"))
			{
				if(listView.SelectedItems==null)
					return;
				menuRefresh.Visible=true;
			}
			if(this.tvComputerCluster.SelectedNode.Parent.Text.Equals("Database"))
			{
				if(listView.SelectedItems==null)
					return;
				menuStartProcess.Visible=true;
				menuStopProcess.Visible=true;
				menuRestartProcess.Visible=true;
				menuRemoveProcess.Visible=true;
				menuRefresh.Visible=true;
				menuStopProcess.Enabled=true;
				menuStartProcess.Enabled=true;
				menuRestartProcess.Enabled=true;
				menuRemoveProcess.Enabled=true;
				menuRefresh.Enabled=true;
			}


			computerMenuRemove.Enabled=true;
			computerMenuConnect.Enabled=true;
			computerMenuDisconnect.Enabled=true;
			computerMenuRefresh.Enabled=true;
			string selectedItem="";
			if(listView.SelectedItems.Count>0) 
				selectedItem=listView.FocusedItem.Text.ToString();
		
		
			if(selectedItem.Equals(""))
			{
				computerMenuAdd.Enabled=true;
				computerMenuRemove.Enabled=false;
				computerMenuConnect.Enabled=false;
				computerMenuDisconnect.Enabled=false;
				return;
			} 
			else 
			{
				computerMenuAdd.Enabled=false;
				if(computerMgmt.getStatus(selectedItem).Equals(Computer.Status.Connected)) 
				{
					computerMenuConnect.Enabled=false;
					computerMenuRemove.Enabled=true;
				}
				if(computerMgmt.getStatus(selectedItem).Equals(Computer.Status.Disconnected))
					computerMenuDisconnect.Enabled=false;
			}
		
	
		}

		private void startProcess(object sender, System.EventArgs e)
		{
			if(listView.SelectedItems.Count==0)
				return;

			string computer = listView.SelectedItems[0].SubItems[0].Text.ToString();
			string process = listView.SelectedItems[0].SubItems[2].Text.ToString();
			
			if(computerMgmt.getComputer(computer).getProcessByName(process).getStatus()==Process.Status.Running)
			{
				MessageBox.Show(this,"The process is already started!"  ,"Process failed to start",MessageBoxButtons.OK);
				return;
			}

			int status = startProcess(listView.SelectedItems[0].SubItems[0].Text.ToString(),listView.SelectedItems[0].SubItems[2].Text.ToString());
			

			if(status < 0)
				MessageBox.Show(this,"Either the link is not OK, or the process is misconfigured! Status : " + status,"Process failed to start",MessageBoxButtons.OK);
			else
				MessageBox.Show(this,"The process was sucessfully started!","Process started",MessageBoxButtons.OK);
				
		}

		private int startProcess(string computer, string process)
		{
			Computer c=computerMgmt.getComputer(computer);
			int status = c.startProcess(c.getProcessByName(process));
			return status;			
		}

		private void listView_ColumnClick_1(object sender, System.Windows.Forms.ColumnClickEventArgs e)
		{	
		//	if(listView.Columns[e.Column].Text.Equals("Computer"))
		//	{
				if(listView.Sorting.Equals(SortOrder.Ascending))
				{
					listView.Sorting=SortOrder.Descending;
				} 
				else
				{
					listView.Sorting=SortOrder.Ascending;
				}
		//	}
		}

		private void removeProcess(object sender, System.EventArgs e)
		{
			if(listView.SelectedItems.Count==0)
				return;
			string process = listView.SelectedItems[0].SubItems[2].Text.ToString();
			string computer = listView.SelectedItems[0].SubItems[0].Text.ToString();
			
			if(MessageBox.Show("Are you sure that you want to remove " +  process + " permanently?","Remove process",MessageBoxButtons.YesNo) == DialogResult.No)
				return;
			removeProcess(computer,process);
			MessageBox.Show(this,"The process was sucessfully removed!","Remove process",MessageBoxButtons.OK);
		}

		private void removeProcess(string computer, string process)
		{
			
			Computer c=computerMgmt.getComputer(computer);
			stopProcess(computer,process);
			int status = c.undefineProcess(c.getProcessByName(process));
			//if(status < 0)
			//		MessageBox.Show(this,"The process could not be removed!","Failed to remove process",MessageBoxButtons.OK);
			//		else 
			//		{
			Database db = computerMgmt.getDatabase((c.getProcessByName(process).getDatabase()));
			db.removeProcess(process);
			c.removeProcess(process,db.getName());
			updateListViews("Database");
			//		}
		}

		private void stopProcess(object sender, System.EventArgs e)
		{
			if(listView.SelectedItems.Count==0)
				return;
			string computer = listView.SelectedItems[0].SubItems[0].Text.ToString();
			string process = listView.SelectedItems[0].SubItems[2].Text.ToString();
			if(computerMgmt.getComputer(computer).getProcessByName(process).getStatus()==Process.Status.Stopped)
			{
				MessageBox.Show(this,"The process is already stopped!"  ,"Process failed to stop",MessageBoxButtons.OK);
				return;
			}

			if(DialogResult.No==MessageBox.Show(this,"Are you sure you want to stop the " + process + " process?","Stop process!", MessageBoxButtons.YesNo))
				return;

			int status = stopProcess(computer, process);
			if(status < 0)
				MessageBox.Show(this,"The process could not be stopped. Status: " + status ,"Process failed to stop",MessageBoxButtons.OK);
			else
				MessageBox.Show(this,"The process was sucessfully stopped!","Process stopped",MessageBoxButtons.OK);
		}

		private int stopProcess(string computer, string process)
		{
			Computer c=computerMgmt.getComputer(computer);
			int status = c.stopProcess(c.getProcessByName(process));
			return status;
		}

		private void restartProcess(object sender, System.EventArgs e)
		{
			if(listView.SelectedItems.Count==0)
				return;
			string computer = listView.SelectedItems[0].SubItems[0].Text.ToString();
			string process = listView.SelectedItems[0].SubItems[2].Text.ToString();
			if(stopProcess(computer, process)<0)
			{
				MessageBox.Show("Restart process failed!!!", "Restart process");
				return;
			}
			if(startProcess(computer, process)<0)
			{
				MessageBox.Show("Restart process failed!!!", "Restart process");
				return;
			}
			MessageBox.Show("Succesfully restarted the process!","Restart process"); 
		}

		private void menuRefresh_Click(object sender, System.EventArgs e)
		{
			//string computer = tvComputerCluster.SelectedNode.Text;
			
			this.listProcesses();
		}

		private void importHostFile(object sender, System.EventArgs e) 
		{
			openHostFileDialog.ShowDialog();	
		}
		
		private void exportHostFile(object sender, System.EventArgs e) 
		{
			saveHostFileDialog.ShowDialog();
		}

		private void listProcesses()
		{
			/* add process in computer list*/
			ArrayList computers = computerMgmt.getComputerCollection();
			foreach(Computer c in computers) 
			{
				c.listProcesses();
				ArrayList processes = c.getProcesses();
				if(processes!=null) 
				{
					foreach(Process p in processes)
					{
						Database db = computerMgmt.getDatabase(p.getDatabase());
						if(db!=null) 
						{
							p.setDefined(true);
							db.addProcessCheck(p);
						}
					}
				}
			}
			updateListViews("Computer");
			updateListViews("Database");
		}

		private void openHostFileDialog_FileOk(object sender, System.ComponentModel.CancelEventArgs e)
		{
			computerMgmt.importHostFile(openHostFileDialog.FileName);
			this.updateTreeViews();
			openHostFileDialog.Dispose();
			listProcesses();
		}

		private void saveHostFileDialog_FileOk(object sender, System.ComponentModel.CancelEventArgs e)
		{
			computerMgmt.exportHostFile(saveHostFileDialog.FileName);
			saveHostFileDialog.Dispose();
		}

		private void mgmConsole_Enter(object sender, System.EventArgs e)
		{/*
			//telnetclient.telnetClient tc= new telnetclient.telnetClient("10.0.13.1",10000,mgmConsole);
			socketcomm.SocketComm sc = new socketcomm.SocketComm("10.0.13.1",10000);
			sc.doConnect();
			while(!sc.isConnected())
			{
				Thread.Sleep(100);
			}
			sc.writeMessage("get status\r");
			string line = sc.readLine();
			while(!line.Equals("")) 
			{
				MessageBox.Show(line);
				line=sc.readLine();
			}
*/
		}

		private void mgmConsole_TextChanged(object sender, System.EventArgs e)
		{
		
		}
		


	
		

		

		
	}

}
