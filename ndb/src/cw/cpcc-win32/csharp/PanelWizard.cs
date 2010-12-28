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

//author:Arun
//date:Nov 13,2002
//Wizard using panel
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;

namespace NDB_CPC
{
	/// <summary>
	/// Summary description for MDXQueryBuilderWizard.
	/// </summary>
	public class PanelWizard : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Button btnCancel;
		private System.Windows.Forms.Button btnback;
		private System.Windows.Forms.Button btnNext;
		private System.Windows.Forms.Button btnFinish;
		
		//---enabling and disabling the buttons
		private bool cancelEnabled;
		private bool backEnabled;
		private bool nextEnabled;
		private bool finishEnabled;
		//--------
		//--set the next and back panel
		private Panel nextPanel;
		private Panel backPanel;
		private Panel presentPanel;
		//
		private Panel[] arrayPanel;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.Panel panel2;
		private System.Windows.Forms.Panel panel3;
		private System.Windows.Forms.RadioButton radioBtnYes;
		private System.Windows.Forms.RadioButton radioBtnNo;
		private System.Windows.Forms.ListBox listBoxComputers;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.Label label1;
		private System.ComponentModel.IContainer components;
		private System.Windows.Forms.Button buttonComputerAdd;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.Label label7;
		private System.Windows.Forms.ComboBox comboNDB;
		private System.Windows.Forms.ComboBox comboAPI;
		private System.Windows.Forms.Label label8;
		private System.Windows.Forms.ComboBox comboMGM;
		private System.Windows.Forms.Button btnTransferNodeToComp;
		private System.Windows.Forms.TreeView tvComputer;
		private System.Windows.Forms.ListView lvNode;
		private System.Windows.Forms.Button btnTransferCompToNode;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Label label9;
		private System.Windows.Forms.Label label10;
		private int m_nMGM;
		private ComputerMgmt mgmt;
		private int m_nNDB;
		private int m_nAPI;
		private Database m_db;
		private System.Windows.Forms.Label label11;
		private System.Windows.Forms.TextBox textDbName;
		private System.Windows.Forms.Label label31;
		private System.Windows.Forms.Label label32;
		private System.Windows.Forms.Label label33;
		private System.Windows.Forms.Label label18;
		private System.Windows.Forms.Label labelTitle;
		private System.Windows.Forms.Label labelCwd;
		private System.Windows.Forms.Label labelArgs;
		private System.Windows.Forms.Label labelOther;
		private System.Windows.Forms.Label labelPath;
		private int m_noOfConfiguredNodes;
		private int m_noOfConfiguredMgmt;
		private int m_noOfConfiguredNdb;
		private string m_mgmHost;
		private string m_mgmPort;
		private System.Windows.Forms.TextBox textCwd;
		private System.Windows.Forms.TextBox textArgs;
		private System.Windows.Forms.TextBox textOther;
		private System.Windows.Forms.TextBox textPath;
		private System.Windows.Forms.TextBox textComputer;
		private System.Windows.Forms.TextBox textDatabase;
		private System.Windows.Forms.TextBox textName;
		private int m_noOfConfiguredApi;
		private bool m_bMgmt;
		private System.Windows.Forms.Button buttonSave;
		private System.Windows.Forms.CheckBox checkBoxReuse;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Panel panel4;
		private System.Windows.Forms.CheckBox checkBoxLater;
		private System.Windows.Forms.RadioButton radioYes;
		private System.Windows.Forms.RadioButton radioNo;
		private System.Windows.Forms.Panel panel6;
		private System.Windows.Forms.Panel panel5;
		private System.Windows.Forms.RadioButton radioStartNo;
		private System.Windows.Forms.RadioButton radioStartYes;
		private System.Windows.Forms.ImageList imageListComp;
		private System.Windows.Forms.Label label12;
		private System.Windows.Forms.TextBox textOwner;
		private System.Windows.Forms.Label label13;
		private System.Windows.Forms.TextBox textEnv;
		private bool m_bNdb;
		public PanelWizard(ComputerMgmt comp)
		{
			mgmt=comp;
			m_noOfConfiguredNodes=0;
			m_noOfConfiguredMgmt=0;
			m_noOfConfiguredNdb=0;
			m_noOfConfiguredApi=0;	
			Size panelSize= new Size(350,300);
			Size s= new Size(355,360);
			Point cancel= new Point(8,310);
			Point back= new Point(96,310);
			Point next = new Point(184,310);
			Point finish= new Point(272,310);
			InitializeComponent();
			this.Size=s;
			this.btnCancel.Location=cancel;
			
			this.btnback.Location=back;
			this.btnNext.Location=next;
			this.btnFinish.Location=finish;

			arrayPanel=new Panel[]{panel1,panel2,panel3,panel4,panel5,panel6};//,panel5, panel6};
			panel1.Size=panelSize;
			
			comboNDB.SelectedIndex=0;
			comboAPI.SelectedIndex=0;
			comboMGM.SelectedIndex=0;
			m_bMgmt=false;
			m_bNdb=false;
			
			m_db = new Database();
			if(listBoxComputers.Items.Count.Equals(0))
				btnNext.Enabled=false;
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
			}
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
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(PanelWizard));
			this.panel1 = new System.Windows.Forms.Panel();
			this.buttonComputerAdd = new System.Windows.Forms.Button();
			this.label1 = new System.Windows.Forms.Label();
			this.label5 = new System.Windows.Forms.Label();
			this.listBoxComputers = new System.Windows.Forms.ListBox();
			this.radioBtnNo = new System.Windows.Forms.RadioButton();
			this.radioBtnYes = new System.Windows.Forms.RadioButton();
			this.panel2 = new System.Windows.Forms.Panel();
			this.label12 = new System.Windows.Forms.Label();
			this.textOwner = new System.Windows.Forms.TextBox();
			this.label11 = new System.Windows.Forms.Label();
			this.textDbName = new System.Windows.Forms.TextBox();
			this.label8 = new System.Windows.Forms.Label();
			this.label7 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.comboMGM = new System.Windows.Forms.ComboBox();
			this.comboAPI = new System.Windows.Forms.ComboBox();
			this.comboNDB = new System.Windows.Forms.ComboBox();
			this.label2 = new System.Windows.Forms.Label();
			this.panel3 = new System.Windows.Forms.Panel();
			this.checkBoxLater = new System.Windows.Forms.CheckBox();
			this.label10 = new System.Windows.Forms.Label();
			this.label9 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.btnTransferCompToNode = new System.Windows.Forms.Button();
			this.btnTransferNodeToComp = new System.Windows.Forms.Button();
			this.lvNode = new System.Windows.Forms.ListView();
			this.tvComputer = new System.Windows.Forms.TreeView();
			this.imageListComp = new System.Windows.Forms.ImageList(this.components);
			this.panel6 = new System.Windows.Forms.Panel();
			this.radioStartNo = new System.Windows.Forms.RadioButton();
			this.radioStartYes = new System.Windows.Forms.RadioButton();
			this.label18 = new System.Windows.Forms.Label();
			this.btnCancel = new System.Windows.Forms.Button();
			this.btnback = new System.Windows.Forms.Button();
			this.btnNext = new System.Windows.Forms.Button();
			this.btnFinish = new System.Windows.Forms.Button();
			this.panel4 = new System.Windows.Forms.Panel();
			this.textEnv = new System.Windows.Forms.TextBox();
			this.label13 = new System.Windows.Forms.Label();
			this.checkBoxReuse = new System.Windows.Forms.CheckBox();
			this.buttonSave = new System.Windows.Forms.Button();
			this.labelTitle = new System.Windows.Forms.Label();
			this.textComputer = new System.Windows.Forms.TextBox();
			this.textCwd = new System.Windows.Forms.TextBox();
			this.textArgs = new System.Windows.Forms.TextBox();
			this.textOther = new System.Windows.Forms.TextBox();
			this.textPath = new System.Windows.Forms.TextBox();
			this.textDatabase = new System.Windows.Forms.TextBox();
			this.textName = new System.Windows.Forms.TextBox();
			this.labelCwd = new System.Windows.Forms.Label();
			this.labelArgs = new System.Windows.Forms.Label();
			this.labelOther = new System.Windows.Forms.Label();
			this.labelPath = new System.Windows.Forms.Label();
			this.label31 = new System.Windows.Forms.Label();
			this.label32 = new System.Windows.Forms.Label();
			this.label33 = new System.Windows.Forms.Label();
			this.panel5 = new System.Windows.Forms.Panel();
			this.radioNo = new System.Windows.Forms.RadioButton();
			this.radioYes = new System.Windows.Forms.RadioButton();
			this.label4 = new System.Windows.Forms.Label();
			this.panel1.SuspendLayout();
			this.panel2.SuspendLayout();
			this.panel3.SuspendLayout();
			this.panel6.SuspendLayout();
			this.panel4.SuspendLayout();
			this.panel5.SuspendLayout();
			this.SuspendLayout();
			// 
			// panel1
			// 
			this.panel1.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.buttonComputerAdd,
																				 this.label1,
																				 this.label5,
																				 this.listBoxComputers,
																				 this.radioBtnNo,
																				 this.radioBtnYes});
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(344, 312);
			this.panel1.TabIndex = 0;
			this.panel1.Paint += new System.Windows.Forms.PaintEventHandler(this.panel1_Paint);
			// 
			// buttonComputerAdd
			// 
			this.buttonComputerAdd.Enabled = false;
			this.buttonComputerAdd.Location = new System.Drawing.Point(192, 232);
			this.buttonComputerAdd.Name = "buttonComputerAdd";
			this.buttonComputerAdd.Size = new System.Drawing.Size(96, 24);
			this.buttonComputerAdd.TabIndex = 3;
			this.buttonComputerAdd.Text = "Add computer...";
			this.buttonComputerAdd.Click += new System.EventHandler(this.buttonComputerAdd_Click);
			// 
			// label1
			// 
			this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.label1.Location = new System.Drawing.Point(80, 8);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(200, 23);
			this.label1.TabIndex = 5;
			this.label1.Text = "Configure computers";
			this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// label5
			// 
			this.label5.Location = new System.Drawing.Point(24, 40);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(128, 23);
			this.label5.TabIndex = 4;
			this.label5.Text = "Available computers:";
			this.label5.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// listBoxComputers
			// 
			this.listBoxComputers.Location = new System.Drawing.Point(24, 64);
			this.listBoxComputers.Name = "listBoxComputers";
			this.listBoxComputers.Size = new System.Drawing.Size(128, 212);
			this.listBoxComputers.TabIndex = 3;
			this.listBoxComputers.SelectedIndexChanged += new System.EventHandler(this.listBoxComputers_SelectedIndexChanged);
			// 
			// radioBtnNo
			// 
			this.radioBtnNo.AutoCheck = false;
			this.radioBtnNo.Location = new System.Drawing.Point(168, 168);
			this.radioBtnNo.Name = "radioBtnNo";
			this.radioBtnNo.Size = new System.Drawing.Size(152, 64);
			this.radioBtnNo.TabIndex = 2;
			this.radioBtnNo.Text = "No, I have to add more computers in order to deploy NDB Cluster. ";
			this.radioBtnNo.Click += new System.EventHandler(this.radioBtnNo_Click);
			// 
			// radioBtnYes
			// 
			this.radioBtnYes.AutoCheck = false;
			this.radioBtnYes.Location = new System.Drawing.Point(168, 72);
			this.radioBtnYes.Name = "radioBtnYes";
			this.radioBtnYes.Size = new System.Drawing.Size(152, 80);
			this.radioBtnYes.TabIndex = 1;
			this.radioBtnYes.Text = "Yes, all the computers that I need to deploy NDB Cluster exists in the list \"Avai" +
				"lable computers\"";
			this.radioBtnYes.Click += new System.EventHandler(this.radioBtnYes_Click);
			// 
			// panel2
			// 
			this.panel2.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.label12,
																				 this.textOwner,
																				 this.label11,
																				 this.textDbName,
																				 this.label8,
																				 this.label7,
																				 this.label6,
																				 this.comboMGM,
																				 this.comboAPI,
																				 this.comboNDB,
																				 this.label2});
			this.panel2.Location = new System.Drawing.Point(0, 320);
			this.panel2.Name = "panel2";
			this.panel2.Size = new System.Drawing.Size(344, 312);
			this.panel2.TabIndex = 1;
			this.panel2.Validating += new System.ComponentModel.CancelEventHandler(this.panel2_Validating);
			this.panel2.Paint += new System.Windows.Forms.PaintEventHandler(this.panel2_Paint);
			// 
			// label12
			// 
			this.label12.Location = new System.Drawing.Point(72, 216);
			this.label12.Name = "label12";
			this.label12.Size = new System.Drawing.Size(112, 24);
			this.label12.TabIndex = 16;
			this.label12.Text = "Database owner:";
			this.label12.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			// 
			// textOwner
			// 
			this.textOwner.Location = new System.Drawing.Point(192, 216);
			this.textOwner.Name = "textOwner";
			this.textOwner.TabIndex = 5;
			this.textOwner.Text = "";
			this.textOwner.TextChanged += new System.EventHandler(this.textOwner_TextChanged);
			// 
			// label11
			// 
			this.label11.Location = new System.Drawing.Point(72, 184);
			this.label11.Name = "label11";
			this.label11.Size = new System.Drawing.Size(112, 24);
			this.label11.TabIndex = 14;
			this.label11.Text = "Database name:";
			this.label11.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.label11.Click += new System.EventHandler(this.label11_Click);
			// 
			// textDbName
			// 
			this.textDbName.Location = new System.Drawing.Point(192, 184);
			this.textDbName.Name = "textDbName";
			this.textDbName.TabIndex = 4;
			this.textDbName.Text = "";
			
			this.textDbName.TextChanged += new System.EventHandler(this.textDbName_TextChanged);
			// 
			// label8
			// 
			this.label8.Location = new System.Drawing.Point(16, 120);
			this.label8.Name = "label8";
			this.label8.Size = new System.Drawing.Size(176, 24);
			this.label8.TabIndex = 12;
			this.label8.Text = "Number of management servers:";
			this.label8.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// label7
			// 
			this.label7.Location = new System.Drawing.Point(16, 88);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(120, 24);
			this.label7.TabIndex = 11;
			this.label7.Text = "Number of API nodes:";
			this.label7.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// label6
			// 
			this.label6.Location = new System.Drawing.Point(16, 56);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(144, 24);
			this.label6.TabIndex = 10;
			this.label6.Text = "Number of database nodes:";
			this.label6.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// comboMGM
			// 
			this.comboMGM.DisplayMember = "0";
			this.comboMGM.Items.AddRange(new object[] {
														  "1"});
			this.comboMGM.Location = new System.Drawing.Point(192, 120);
			this.comboMGM.Name = "comboMGM";
			this.comboMGM.Size = new System.Drawing.Size(104, 21);
			this.comboMGM.TabIndex = 3;
			this.comboMGM.Text = "comboBox3";
			// 
			// comboAPI
			// 
			this.comboAPI.DisplayMember = "0";
			this.comboAPI.Items.AddRange(new object[] {
														  "1",
														  "2",
														  "3",
														  "4",
														  "5",
														  "6",
														  "7",
														  "8",
														  "9",
														  "10"});
			this.comboAPI.Location = new System.Drawing.Point(192, 88);
			this.comboAPI.Name = "comboAPI";
			this.comboAPI.Size = new System.Drawing.Size(104, 21);
			this.comboAPI.TabIndex = 2;
			this.comboAPI.Text = "comboBox2";
			// 
			// comboNDB
			// 
			this.comboNDB.DisplayMember = "0";
			this.comboNDB.Items.AddRange(new object[] {
														  "1",
														  "2",
														  "4",
														  "8"});
			this.comboNDB.Location = new System.Drawing.Point(192, 56);
			this.comboNDB.Name = "comboNDB";
			this.comboNDB.Size = new System.Drawing.Size(104, 21);
			this.comboNDB.TabIndex = 1;
			this.comboNDB.Text = "comboBox1";
			// 
			// label2
			// 
			this.label2.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.label2.Location = new System.Drawing.Point(80, 8);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(208, 23);
			this.label2.TabIndex = 6;
			this.label2.Text = "Setup NDB Cluster nodes";
			this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// panel3
			// 
			this.panel3.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.checkBoxLater,
																				 this.label10,
																				 this.label9,
																				 this.label3,
																				 this.btnTransferCompToNode,
																				 this.btnTransferNodeToComp,
																				 this.lvNode,
																				 this.tvComputer});
			this.panel3.Location = new System.Drawing.Point(360, 8);
			this.panel3.Name = "panel3";
			this.panel3.Size = new System.Drawing.Size(320, 312);
			this.panel3.TabIndex = 2;
			// 
			// checkBoxLater
			// 
			this.checkBoxLater.Location = new System.Drawing.Point(40, 256);
			this.checkBoxLater.Name = "checkBoxLater";
			this.checkBoxLater.Size = new System.Drawing.Size(240, 16);
			this.checkBoxLater.TabIndex = 9;
			this.checkBoxLater.Text = "I will configure these nodes manually, later.";
			// 
			// label10
			// 
			this.label10.Location = new System.Drawing.Point(16, 40);
			this.label10.Name = "label10";
			this.label10.Size = new System.Drawing.Size(104, 16);
			this.label10.TabIndex = 8;
			this.label10.Text = "NDB Cluster nodes:";
			this.label10.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// label9
			// 
			this.label9.Location = new System.Drawing.Point(192, 40);
			this.label9.Name = "label9";
			this.label9.Size = new System.Drawing.Size(100, 16);
			this.label9.TabIndex = 7;
			this.label9.Text = "Computers:";
			this.label9.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
			// 
			// label3
			// 
			this.label3.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.label3.Location = new System.Drawing.Point(40, 8);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(280, 23);
			this.label3.TabIndex = 6;
			this.label3.Text = "Assign NDB nodes to computers";
			this.label3.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// btnTransferCompToNode
			// 
			this.btnTransferCompToNode.Location = new System.Drawing.Point(144, 160);
			this.btnTransferCompToNode.Name = "btnTransferCompToNode";
			this.btnTransferCompToNode.Size = new System.Drawing.Size(40, 24);
			this.btnTransferCompToNode.TabIndex = 4;
			this.btnTransferCompToNode.Text = "<---";
			// 
			// btnTransferNodeToComp
			// 
			this.btnTransferNodeToComp.Location = new System.Drawing.Point(144, 128);
			this.btnTransferNodeToComp.Name = "btnTransferNodeToComp";
			this.btnTransferNodeToComp.Size = new System.Drawing.Size(40, 24);
			this.btnTransferNodeToComp.TabIndex = 3;
			this.btnTransferNodeToComp.Text = "--->";
			this.btnTransferNodeToComp.Click += new System.EventHandler(this.btnTransferNodeToComp_Click);
			// 
			// lvNode
			// 
			this.lvNode.HideSelection = false;
			this.lvNode.Location = new System.Drawing.Point(16, 56);
			this.lvNode.Name = "lvNode";
			this.lvNode.Size = new System.Drawing.Size(112, 192);
			this.lvNode.TabIndex = 2;
			this.lvNode.View = System.Windows.Forms.View.List;
			this.lvNode.SelectedIndexChanged += new System.EventHandler(this.lvNode_SelectedIndexChanged);
			// 
			// tvComputer
			// 
			this.tvComputer.HideSelection = false;
			this.tvComputer.ImageList = this.imageListComp;
			this.tvComputer.Location = new System.Drawing.Point(192, 56);
			this.tvComputer.Name = "tvComputer";
			this.tvComputer.Size = new System.Drawing.Size(120, 192);
			this.tvComputer.TabIndex = 1;
			this.tvComputer.MouseDown += new System.Windows.Forms.MouseEventHandler(this.tvComputer_MouseDown);
			this.tvComputer.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.tvComputer_AfterSelect);
			this.tvComputer.MouseLeave += new System.EventHandler(this.tvComputer_MouseLeave);
			this.tvComputer.DragDrop += new System.Windows.Forms.DragEventHandler(this.tvComputer_DragDrop);
			// 
			// imageListComp
			// 
			this.imageListComp.ColorDepth = System.Windows.Forms.ColorDepth.Depth8Bit;
			this.imageListComp.ImageSize = new System.Drawing.Size(16, 16);
			this.imageListComp.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageListComp.ImageStream")));
			this.imageListComp.TransparentColor = System.Drawing.Color.Transparent;
			// 
			// panel6
			// 
			this.panel6.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.radioStartNo,
																				 this.radioStartYes,
																				 this.label18});
			this.panel6.Location = new System.Drawing.Point(344, 336);
			this.panel6.Name = "panel6";
			this.panel6.Size = new System.Drawing.Size(344, 312);
			this.panel6.TabIndex = 3;
			this.panel6.Paint += new System.Windows.Forms.PaintEventHandler(this.panel4_Paint);
			// 
			// radioStartNo
			// 
			this.radioStartNo.Location = new System.Drawing.Point(40, 144);
			this.radioStartNo.Name = "radioStartNo";
			this.radioStartNo.Size = new System.Drawing.Size(272, 48);
			this.radioStartNo.TabIndex = 81;
			this.radioStartNo.Text = "Manually start NDB Cluster. The Magician will exit and you must start NDB Cluster" +
				" manually.";
			this.radioStartNo.CheckedChanged += new System.EventHandler(this.radioStartNo_CheckedChanged);
			// 
			// radioStartYes
			// 
			this.radioStartYes.Location = new System.Drawing.Point(40, 40);
			this.radioStartYes.Name = "radioStartYes";
			this.radioStartYes.Size = new System.Drawing.Size(272, 88);
			this.radioStartYes.TabIndex = 80;
			this.radioStartYes.Text = "Start NDB Cluster now. The Magician will start NDB Cluster and exit. MAKE SURE YO" +
				"U HAVE STARTED THE MGMTSRVR WITH THE CORRECT CONFIGURATION FILE!!!";
			this.radioStartYes.CheckedChanged += new System.EventHandler(this.radioStartYes_CheckedChanged);
			// 
			// label18
			// 
			this.label18.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.label18.Location = new System.Drawing.Point(56, 8);
			this.label18.Name = "label18";
			this.label18.Size = new System.Drawing.Size(224, 24);
			this.label18.TabIndex = 79;
			this.label18.Text = "Start NDB Cluster and finish";
			// 
			// btnCancel
			// 
			this.btnCancel.Location = new System.Drawing.Point(8, 656);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.Size = new System.Drawing.Size(70, 23);
			this.btnCancel.TabIndex = 10;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
			// 
			// btnback
			// 
			this.btnback.Location = new System.Drawing.Point(96, 656);
			this.btnback.Name = "btnback";
			this.btnback.Size = new System.Drawing.Size(70, 23);
			this.btnback.TabIndex = 11;
			this.btnback.Text = "< Back";
			this.btnback.Click += new System.EventHandler(this.btnback_Click);
			// 
			// btnNext
			// 
			this.btnNext.Location = new System.Drawing.Point(184, 656);
			this.btnNext.Name = "btnNext";
			this.btnNext.Size = new System.Drawing.Size(70, 23);
			this.btnNext.TabIndex = 12;
			this.btnNext.Text = "Next >";
			this.btnNext.Click += new System.EventHandler(this.btnNext_Click);
			// 
			// btnFinish
			// 
			this.btnFinish.Location = new System.Drawing.Point(272, 656);
			this.btnFinish.Name = "btnFinish";
			this.btnFinish.Size = new System.Drawing.Size(70, 23);
			this.btnFinish.TabIndex = 13;
			this.btnFinish.Text = "Finish";
			this.btnFinish.Click += new System.EventHandler(this.btnFinish_Click);
			// 
			// panel4
			// 
			this.panel4.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.textEnv,
																				 this.label13,
																				 this.checkBoxReuse,
																				 this.buttonSave,
																				 this.labelTitle,
																				 this.textComputer,
																				 this.textCwd,
																				 this.textArgs,
																				 this.textOther,
																				 this.textPath,
																				 this.textDatabase,
																				 this.textName,
																				 this.labelCwd,
																				 this.labelArgs,
																				 this.labelOther,
																				 this.labelPath,
																				 this.label31,
																				 this.label32,
																				 this.label33});
			this.panel4.Location = new System.Drawing.Point(672, 8);
			this.panel4.Name = "panel4";
			this.panel4.Size = new System.Drawing.Size(344, 312);
			this.panel4.TabIndex = 62;
			this.panel4.Paint += new System.Windows.Forms.PaintEventHandler(this.panel5_Paint);
			// 
			// textEnv
			// 
			this.textEnv.Location = new System.Drawing.Point(136, 136);
			this.textEnv.Name = "textEnv";
			this.textEnv.Size = new System.Drawing.Size(184, 20);
			this.textEnv.TabIndex = 2;
			this.textEnv.TabStop = false;
			this.textEnv.Text = "";
			// 
			// label13
			// 
			this.label13.Location = new System.Drawing.Point(8, 136);
			this.label13.Name = "label13";
			this.label13.Size = new System.Drawing.Size(136, 24);
			this.label13.TabIndex = 81;
			this.label13.Text = "Environment variables:";
			// 
			// checkBoxReuse
			// 
			this.checkBoxReuse.Location = new System.Drawing.Point(88, 232);
			this.checkBoxReuse.Name = "checkBoxReuse";
			this.checkBoxReuse.Size = new System.Drawing.Size(240, 32);
			this.checkBoxReuse.TabIndex = 5;
			this.checkBoxReuse.TabStop = false;
			this.checkBoxReuse.Text = "Use the same configuration for ALL NDB nodes?";
			// 
			// buttonSave
			// 
			this.buttonSave.Location = new System.Drawing.Point(184, 264);
			this.buttonSave.Name = "buttonSave";
			this.buttonSave.Size = new System.Drawing.Size(88, 24);
			this.buttonSave.TabIndex = 6;
			this.buttonSave.Text = "Save";
			this.buttonSave.Click += new System.EventHandler(this.buttonSave_Click);
			// 
			// labelTitle
			// 
			this.labelTitle.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.labelTitle.Location = new System.Drawing.Point(80, 16);
			this.labelTitle.Name = "labelTitle";
			this.labelTitle.Size = new System.Drawing.Size(192, 23);
			this.labelTitle.TabIndex = 79;
			this.labelTitle.Text = "Mgmtsrvr configuration";
			// 
			// textComputer
			// 
			this.textComputer.Location = new System.Drawing.Point(136, 40);
			this.textComputer.Name = "textComputer";
			this.textComputer.ReadOnly = true;
			this.textComputer.Size = new System.Drawing.Size(184, 20);
			this.textComputer.TabIndex = 77;
			this.textComputer.TabStop = false;
			this.textComputer.Text = "";
			// 
			// textCwd
			// 
			this.textCwd.Location = new System.Drawing.Point(136, 208);
			this.textCwd.Name = "textCwd";
			this.textCwd.Size = new System.Drawing.Size(184, 20);
			this.textCwd.TabIndex = 5;
			this.textCwd.TabStop = false;
			this.textCwd.Text = "";
			// 
			// textArgs
			// 
			this.textArgs.Location = new System.Drawing.Point(136, 184);
			this.textArgs.Name = "textArgs";
			this.textArgs.Size = new System.Drawing.Size(184, 20);
			this.textArgs.TabIndex = 4;
			this.textArgs.TabStop = false;
			this.textArgs.Text = "";
			// 
			// textOther
			// 
			this.textOther.Location = new System.Drawing.Point(136, 160);
			this.textOther.Name = "textOther";
			this.textOther.Size = new System.Drawing.Size(184, 20);
			this.textOther.TabIndex = 3;
			this.textOther.TabStop = false;
			this.textOther.Text = "";
			// 
			// textPath
			// 
			this.textPath.Location = new System.Drawing.Point(136, 112);
			this.textPath.Name = "textPath";
			this.textPath.Size = new System.Drawing.Size(184, 20);
			this.textPath.TabIndex = 1;
			this.textPath.TabStop = false;
			this.textPath.Text = "";
			this.textPath.TextChanged += new System.EventHandler(this.textPath_TextChanged);
			// 
			// textDatabase
			// 
			this.textDatabase.Location = new System.Drawing.Point(136, 88);
			this.textDatabase.Name = "textDatabase";
			this.textDatabase.ReadOnly = true;
			this.textDatabase.Size = new System.Drawing.Size(184, 20);
			this.textDatabase.TabIndex = 62;
			this.textDatabase.TabStop = false;
			this.textDatabase.Text = "";
			// 
			// textName
			// 
			this.textName.Location = new System.Drawing.Point(136, 64);
			this.textName.Name = "textName";
			this.textName.ReadOnly = true;
			this.textName.Size = new System.Drawing.Size(184, 20);
			this.textName.TabIndex = 60;
			this.textName.TabStop = false;
			this.textName.Text = "";
			// 
			// labelCwd
			// 
			this.labelCwd.Location = new System.Drawing.Point(8, 208);
			this.labelCwd.Name = "labelCwd";
			this.labelCwd.Size = new System.Drawing.Size(112, 24);
			this.labelCwd.TabIndex = 72;
			this.labelCwd.Text = "Current working dir.:";
			// 
			// labelArgs
			// 
			this.labelArgs.Location = new System.Drawing.Point(8, 184);
			this.labelArgs.Name = "labelArgs";
			this.labelArgs.Size = new System.Drawing.Size(128, 24);
			this.labelArgs.TabIndex = 70;
			this.labelArgs.Text = "Arguments to mgmtsrvr:";
			// 
			// labelOther
			// 
			this.labelOther.Location = new System.Drawing.Point(8, 160);
			this.labelOther.Name = "labelOther";
			this.labelOther.Size = new System.Drawing.Size(136, 24);
			this.labelOther.TabIndex = 69;
			this.labelOther.Text = "Mgmtsrvr port:";
			// 
			// labelPath
			// 
			this.labelPath.Location = new System.Drawing.Point(8, 112);
			this.labelPath.Name = "labelPath";
			this.labelPath.Size = new System.Drawing.Size(128, 24);
			this.labelPath.TabIndex = 67;
			this.labelPath.Text = "Path to mgmtsrvr binary:";
			// 
			// label31
			// 
			this.label31.Location = new System.Drawing.Point(8, 88);
			this.label31.Name = "label31";
			this.label31.Size = new System.Drawing.Size(88, 24);
			this.label31.TabIndex = 65;
			this.label31.Text = "Database:";
			// 
			// label32
			// 
			this.label32.Location = new System.Drawing.Point(8, 64);
			this.label32.Name = "label32";
			this.label32.Size = new System.Drawing.Size(88, 24);
			this.label32.TabIndex = 63;
			this.label32.Text = "Process name:";
			// 
			// label33
			// 
			this.label33.Location = new System.Drawing.Point(8, 40);
			this.label33.Name = "label33";
			this.label33.Size = new System.Drawing.Size(64, 24);
			this.label33.TabIndex = 61;
			this.label33.Text = "Computer:";
			// 
			// panel5
			// 
			this.panel5.Controls.AddRange(new System.Windows.Forms.Control[] {
																				 this.radioNo,
																				 this.radioYes,
																				 this.label4});
			this.panel5.Location = new System.Drawing.Point(672, 328);
			this.panel5.Name = "panel5";
			this.panel5.Size = new System.Drawing.Size(344, 312);
			this.panel5.TabIndex = 63;
			// 
			// radioNo
			// 
			this.radioNo.Location = new System.Drawing.Point(72, 160);
			this.radioNo.Name = "radioNo";
			this.radioNo.Size = new System.Drawing.Size(240, 48);
			this.radioNo.TabIndex = 1;
			this.radioNo.Text = "I already have a configuration file that I want to use for this configuration.";
			this.radioNo.CheckedChanged += new System.EventHandler(this.radioNo_CheckedChanged);
			// 
			// radioYes
			// 
			this.radioYes.Checked = true;
			this.radioYes.Location = new System.Drawing.Point(72, 56);
			this.radioYes.Name = "radioYes";
			this.radioYes.Size = new System.Drawing.Size(240, 88);
			this.radioYes.TabIndex = 0;
			this.radioYes.TabStop = true;
			this.radioYes.Text = "Generate a configuration file template (initconfig.txt) for the mgmtsrvr based on" +
				" the specified configuration? Notepad will be started with a template that you m" +
				"ust complete and save in the cwd of the mgmtsrvr.";
			this.radioYes.CheckedChanged += new System.EventHandler(this.radioYes_CheckedChanged);
			// 
			// label4
			// 
			this.label4.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.label4.Location = new System.Drawing.Point(88, 8);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(192, 40);
			this.label4.TabIndex = 79;
			this.label4.Text = "Tying up the configuration";
			this.label4.TextAlign = System.Drawing.ContentAlignment.TopCenter;
			// 
			// PanelWizard
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(1030, 755);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.panel5,
																		  this.panel4,
																		  this.panel1,
																		  this.btnFinish,
																		  this.btnNext,
																		  this.btnback,
																		  this.btnCancel,
																		  this.panel6,
																		  this.panel3,
																		  this.panel2});
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "PanelWizard";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.Text = "Create Database Magician";
			this.Load += new System.EventHandler(this.MDXQueryBuilderWizard_Load);
			this.Activated += new System.EventHandler(this.PanelWizard_Activated);
			this.panel1.ResumeLayout(false);
			this.panel2.ResumeLayout(false);
			this.panel3.ResumeLayout(false);
			this.panel6.ResumeLayout(false);
			this.panel4.ResumeLayout(false);
			this.panel5.ResumeLayout(false);
			this.ResumeLayout(false);

		}
		#endregion

		private void MDXQueryBuilderWizard_Load(object sender, System.EventArgs e)
		{
		
			foreach(Control ct in this.Controls)
			{
				if(ct.GetType().Name=="Panel")
				{
					ct.Left=0;
					ct.Top=0;
					ct.Visible=false;
				}
				
			}
			presentPanel=arrayPanel[0];
			//--set the properties
			setBtnPanProperty(getPosition(presentPanel));
			//------
			refreshLook();
		}

		//-set the buttons and panel
		private void refreshLook()
		{
			if(cancelEnabled)
				btnCancel.Enabled=true;
			else
				btnCancel.Enabled=false;

			if(backEnabled)
				btnback.Enabled=true;
			else
				btnback.Enabled=false;

			if(nextEnabled)
				btnNext.Enabled=true;
			else
				btnNext.Enabled=false;

			if(finishEnabled)
				btnFinish.Enabled=true;
			else
				btnFinish.Enabled=false;
			
			if(presentPanel!=null)
			{
				presentPanel.Show();
				presentPanel.BringToFront();
			}
		}
		//--------
		private int getPosition(Panel p)
		{
			int result=-1;
			for(int i=0;i<arrayPanel.Length;i++)
			{
				if(arrayPanel[i]==p)
				{
					result=i;
					break;
				}
			}
			return result ;
		}
		//----

		private void setBtnPanProperty(int presentPanelPosition )
		{						
			int panelLength=arrayPanel.Length-1;
			if(presentPanelPosition==0)
			{
				//first panel...no back ,no finish
				cancelEnabled=true;
				backEnabled=false;
				nextEnabled=false;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=arrayPanel[presentPanelPosition+1];
				backPanel=null;

			}
			else if(presentPanelPosition==1)
			{
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=false;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=arrayPanel[presentPanelPosition+1];
				backPanel=arrayPanel[presentPanelPosition-1];
			}

			else if(presentPanelPosition==2)
			{
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=false;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=arrayPanel[presentPanelPosition+1];
				backPanel=arrayPanel[presentPanelPosition-1];
			}

			else if(presentPanelPosition==3)
			{
				//last panel...no next,finish
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=true;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=arrayPanel[presentPanelPosition+1];
				backPanel=arrayPanel[presentPanelPosition-1];
			}
			else if(presentPanelPosition==4)
			{
				//last panel...no next,finish
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=true;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=presentPanel;
				backPanel=arrayPanel[presentPanelPosition-1];
			}

			else if(presentPanelPosition==5)
			{
				//last panel...no next,finish yes
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=false;
				finishEnabled=false;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=null;
				backPanel=arrayPanel[presentPanelPosition-1];
			}

			else
			{
				//no finish,next and back
				cancelEnabled=true;
				backEnabled=true;
				nextEnabled=false;
				finishEnabled=true;
				presentPanel=arrayPanel[presentPanelPosition];
				nextPanel=null;
				backPanel=arrayPanel[presentPanelPosition-1];
			}
		}

		private void btnNext_Click(object sender, System.EventArgs e)
		{
			
			if(arrayPanel[getPosition(presentPanel)].Equals(panel1))
			{
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				refreshLook();
				return;
			}
			if(arrayPanel[getPosition(presentPanel)].Equals(panel2))
			{
				m_db.setName(textDbName.Text.ToString());
				m_db.setOwner(textOwner.Text.ToString());
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				//presentPanel
				setBtnPanProperty(getPosition(presentPanel));
				prepareNodeAssignmentPanel();	
				refreshLook();
			
				return;
			}
			if(arrayPanel[getPosition(presentPanel)].Equals(panel3))
			{
				prepareNodeConfigurationPanel();	
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				refreshLook();
				return;
			}
			if(arrayPanel[getPosition(presentPanel)].Equals(panel4))
			{
				nextEnabled=true;
				finishEnabled=true;
				backEnabled=true;
				cancelEnabled=true;
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				refreshLook();
				return;
			}
			if(arrayPanel[getPosition(presentPanel)].Equals(panel5)) 
			{
				generateInitConfig();
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				refreshLook();
				return;
			}
		
			if(arrayPanel[getPosition(presentPanel)].Equals(panel6)) 
			{
			//	presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				refreshLook();
				return;
			}
			/*else 
			{
				presentPanel=arrayPanel[getPosition(presentPanel)+1];
				setBtnPanProperty(getPosition(presentPanel));
				updateListViews();
				refreshLook();
			}*/
		}


		private void btnback_Click(object sender, System.EventArgs e)
		{
			presentPanel=arrayPanel[getPosition(presentPanel)-1];
			setBtnPanProperty(getPosition(presentPanel));
			m_noOfConfiguredNodes=0;
			m_noOfConfiguredNodes=0;
			m_noOfConfiguredMgmt=0;
			m_noOfConfiguredNdb=0;
			m_noOfConfiguredApi=0;
			m_bNdb=false;
			m_bMgmt=false;
			refreshLook();
		}
		

		private void btnCancel_Click(object sender, System.EventArgs e)
		{
			m_db.removeAllProcesses();
			this.Dispose(true);
		}

		

		
		private void radioBtnYes_Click(object sender, System.EventArgs e)
		{
			if(radioBtnNo.Checked.Equals(false))
			{
				if(radioBtnYes.Checked.Equals(true))
					radioBtnYes.Checked=false;
				else 
				{
					radioBtnYes.Checked=true;
					this.btnNext.Enabled=true;
				}
				
			}
			if(radioBtnNo.Checked.Equals(true)) 
			{
				radioBtnNo.Checked=false;
				radioBtnYes.Checked=true;
				buttonComputerAdd.Enabled=false;
				this.btnNext.Enabled=true;
			}
					
		}

		private void radioBtnNo_Click(object sender, System.EventArgs e)
		{
			if(radioBtnYes.Checked.Equals(false))
			{
				if(radioBtnNo.Checked.Equals(true)) 
				{
					radioBtnNo.Checked=false;
					buttonComputerAdd.Enabled=false;
				}
				else 
				{
					radioBtnNo.Checked=true;
					buttonComputerAdd.Enabled=true;
					this.btnNext.Enabled=false;
				}
				
			}
			if(radioBtnYes.Checked.Equals(true)) 
			{
				radioBtnYes.Checked=false;
				radioBtnNo.Checked=true;
				buttonComputerAdd.Enabled=true;
				this.btnNext.Enabled=false;
			}		
		}

		private void buttonComputerAdd_Click(object sender, System.EventArgs e)
		{
			if(getPosition(presentPanel)==0) 
			{
				if(radioBtnNo.Checked.Equals(true))
				{
					ComputerAddDialog cad = new ComputerAddDialog(mgmt);
					cad.ShowDialog();
				}
			}
		}

		private void PanelWizard_Activated(object sender, System.EventArgs e)
		{
			updateComputers();
		}

		private void updateComputers()
		{
			ArrayList list = mgmt.getComputerCollection();
			this.listBoxComputers.BeginUpdate();
			this.listBoxComputers.Items.Clear();
			foreach(Computer c in list) 
			{
				this.listBoxComputers.Items.Add(c.getName());
			}
			if(listBoxComputers.Items.Count > 0) 
			{
				btnNext.Enabled=true;
			}
			this.listBoxComputers.EndUpdate();
			this.listBoxComputers.Refresh();
		}	
		

		private void tvComputer_AfterSelect(object sender, System.Windows.Forms.TreeViewEventArgs e)
		{
			tvComputer.SelectedNode.Expand();

		}

		private void tvComputer_DragDrop(object sender, System.Windows.Forms.DragEventArgs e)
		{
				
		}

		private void tvComputer_MouseLeave(object sender, System.EventArgs e)
		{
		
		}

		private void tvComputer_MouseDown(object sender, System.Windows.Forms.MouseEventArgs e)
		{
			TreeNode prevNode = tvComputer.SelectedNode;
			if(prevNode!=null)
			{
				prevNode.BackColor=Color.White;
			}
			TreeNode node = tvComputer.GetNodeAt(e.X,e.Y);
			if(node==null)
			{
				return;
			}
		
			tvComputer.SelectedNode=node;
			tvComputer.SelectedNode.BackColor=Color.LightGray;
			
		}

		private void btnTransferNodeToComp_Click(object sender, System.EventArgs e)
		{
			
			if(tvComputer.SelectedNode==null)
				return;
			if(lvNode.SelectedItems.Equals(null))
				return;
			int itemCount=lvNode.SelectedItems.Count;
			lvNode.BeginUpdate();
			tvComputer.BeginUpdate();
			for(int i=0;i < itemCount;i++)
			{
				tvComputer.SelectedNode.Nodes.Add(lvNode.SelectedItems[i].Text.ToString());
			}
				
			for(int i=0;i < itemCount;i++)
			{
				lvNode.Items.RemoveAt(lvNode.SelectedIndices[0]);
				
			}
			if(lvNode.Items.Count.Equals(0))
				btnNext.Enabled=true;
			else
				btnNext.Enabled=false;
			tvComputer.SelectedNode.Expand();
			lvNode.EndUpdate();
			tvComputer.EndUpdate();
		}

		private void lvNode_SelectedIndexChanged(object sender, System.EventArgs e)
		{
		}

		private void prepareNodeAssignmentPanel()
		{
			ArrayList computers = mgmt.getComputerCollection();
			m_nNDB=Convert.ToInt32(comboNDB.SelectedItem.ToString());
			m_nAPI=Convert.ToInt32(comboAPI.SelectedItem.ToString());
			m_nMGM=Convert.ToInt32(comboMGM.SelectedItem.ToString());
		
			lvNode.Items.Clear();
			tvComputer.Nodes.Clear();
			for (int i=1;i<=m_nMGM;i++)
				lvNode.Items.Add("mgm."+i);
				
			for (int i=m_nMGM+1;i<=(m_nNDB+m_nMGM);i++)
				lvNode.Items.Add("ndb."+i);
		
			for (int i=m_nMGM+m_nNDB+1;i<=(m_nNDB+m_nMGM+m_nAPI);i++)
				lvNode.Items.Add("api."+i);
		
			foreach(Computer c in computers)
			{
				if(c.getStatus() == Computer.Status.Connected)
					tvComputer.Nodes.Add(c.getName());
			}
		
		}
		private void prepareNodeConfigurationPanel()
		{	
			Computer c;
			for(int i=0;i<tvComputer.Nodes.Count;i++) 
			{
				c=mgmt.getComputer(tvComputer.Nodes[i].Text.ToString());
				for(int j=0; j < tvComputer.Nodes[i].Nodes.Count;j++)
				{
					m_db.addProcess(new Process(tvComputer.Nodes[i].Nodes[j].Text.ToString(),m_db.getOwner(),m_db.getName(),c));
					c.addProcess(m_db.getProcessByName(tvComputer.Nodes[i].Nodes[j].Text.ToString()));
				}
			}
		}

		private void updateListViews()
		{/*
			lvConfig.Items.Clear();
			ArrayList processes = m_db.getProcesses();
			string [] processcols= new string[5];
			foreach (Process process in processes)
			{
				processcols[0]=process.getName();
				processcols[1]=process.getComputer().getName();
				processcols[2]=process.getPath();
				processcols[3]="";
				processcols[4]="";
				
				ListViewItem lvc= new ListViewItem(processcols);
	
				
				lvConfig.Items.Add(lvc);
			}
			lvConfig.EndUpdate();
		*/	
		}

		private void btnConfigure_Click(object sender, System.EventArgs e)
		{
			
		}

		private void textDbName_TextChanged(object sender, System.EventArgs e)
		{
			if(textOwner.TextLength>0 && textDbName.TextLength > 0)
				nextEnabled=true;
			else
				nextEnabled=false;

			refreshLook();
			
		}

		private void checkBoxLater_CheckedChanged(object sender, System.EventArgs e)
		{
			if(checkBoxLater.Checked.Equals(true)) 
			{
				this.finishEnabled=true;
				this.nextEnabled=false;
			} 
			else 
			{
				this.finishEnabled=false;
				this.nextEnabled=true;
			}
			this.refreshLook();
		}

		private void btnFinish_Click(object sender, System.EventArgs e)
		{
			mgmt.AddDatabase(this.m_db);

			if(radioStartYes.Checked==true)
				startDatabase();
			this.Dispose();
		}

		private void panel4_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			
	//		Point location= new Point(8,40);
	//		Size s= new Size(panel4.Size.Width-8,panel4.Size.Height-120);
	//		lvConfig.Location=location;
	//		lvConfig.Size=s;
			

		}	

		private void configureMgmt()
		{
			//clear old
			textOther.Text="";
			textArgs.Text="";
			textCwd.Text="";
			textPath.Text="";

			textPath.Clear();
			textEnv.Clear();
			textOther.Clear();
			textCwd.Clear();
			textArgs.Clear();

			textPath.ClearUndo();
			textEnv.ClearUndo();
			textOther.ClearUndo();
			textCwd.ClearUndo();
			textArgs.ClearUndo();

			
			textOther.Enabled=true;
			textArgs.Enabled=true;
			textCwd.Enabled=true;
			textPath.Enabled=true;

			textPath.TabStop=true;
			textOther.TabStop=true;
			textArgs.TabStop=true;
			textCwd.TabStop=true;
			textEnv.TabStop=true;
			
			labelTitle.Text="Mgmtsrvr configuration";
			labelPath.Text="Path to mgmtsrvr binary:";
			labelArgs.Text="Arguments to mgmtsrvr:";
			labelOther.Text="Mgmtsrvr port (-p X):";

			//get new
			String process="mgm." + Convert.ToString(m_noOfConfiguredMgmt+1);
			Process mgmt=m_db.getProcessByName(process);
			textComputer.Text=mgmt.getComputer().getName();
			textName.Text=mgmt.getName().ToString();
			textDatabase.Text=mgmt.getDatabase().ToString();
			m_mgmHost=mgmt.getComputer().getName();
			textPath.Focus();
		}
		private void configureApi()
		{
			checkBoxReuse.Text="Use the same configuration for ALL API nodes?";
			if(m_nAPI > 1) 
		    {
				checkBoxReuse.Visible=true;
				checkBoxReuse.Enabled=true;
				
			}
			else
			{
				checkBoxReuse.Enabled=false;
				checkBoxReuse.Visible=true;
			}
			
			// clear previous and get a new api
			
			textOther.Text="";
			textArgs.Text="";
			//textCwd.Text="";
			//textPath.Text="";
			//get new api
			textOther.Enabled=false;
			textArgs.Enabled=true;
			labelTitle.Text="API node configuration";
			labelPath.Text="Path to api binary:";
			labelArgs.Text="Arguments to api:";
			labelOther.Text="NDB_CONNECTSTRING";
			String process="api." + Convert.ToString(m_noOfConfiguredApi+m_nMGM+m_nNDB+1);
			Process api=m_db.getProcessByName(process);
			textComputer.Text=api.getComputer().getName();
			textName.Text=api.getName().ToString();
			textOther.Text="nodeid=" + Convert.ToString(m_noOfConfiguredApi+m_nMGM+m_nNDB+1) + ";host="+this.m_mgmHost + ":" + this.m_mgmPort; 
			textDatabase.Text=api.getDatabase().ToString();
			textPath.Focus();
		}

		private void configureNdb()
		{


			checkBoxReuse.Text="Use the same configuration for ALL NDB nodes?";
		

			if(this.m_nNDB > 1) 
			{
				checkBoxReuse.Visible=true;
				checkBoxReuse.Enabled=true;
				
			}
			else
			{
				checkBoxReuse.Enabled=false;
				checkBoxReuse.Visible=true;
			}
			
			

			labelPath.Text="Path to ndb binary:";
			labelArgs.Text="Arguments to ndb:";
			
			// clear previous and get a new ndb
			
			labelOther.Text="NDB_CONNECTSTRING";
			textArgs.Text="-i";
			textOther.Enabled=false;
			textArgs.Enabled=false;

			textPath.TabStop=true;
			textEnv.TabStop=true;
			textOther.TabStop=false;
			textArgs.TabStop=false;
			textCwd.TabStop=true;
			
			//textCwd.Text="";
			//textPath.Text="";
			//get new
			
			String process="ndb." + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1);
			textOther.Text="nodeid=" + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1) + ";host="+this.m_mgmHost + ":" + this.m_mgmPort;
			Process ndb=m_db.getProcessByName(process);
			textComputer.Text=ndb.getComputer().getName();
			textName.Text=ndb.getName().ToString();
			textDatabase.Text=ndb.getDatabase().ToString();
			textPath.Focus();
		}


		public void saveMgm()
		{
			String process="mgm." + Convert.ToString(m_noOfConfiguredMgmt+1);
			Process mgmt=m_db.getProcessByName(process);
			mgmt.setOther(textOther.Text.ToString());
			mgmt.setEnv(textEnv.Text.ToString());
			m_mgmPort = textOther.Text.ToString();
			try 
			{
				m_db.setMgmtPort(Convert.ToInt32(m_mgmPort));
			}
			catch(Exception e)
			{
				MessageBox.Show("Port number must be numeric!!!", "Error",MessageBoxButtons.OK);
				this.configureMgmt();
				return;
			}
			mgmt.setPath(textPath.Text.ToString());
			mgmt.setCwd(textCwd.Text.ToString());
			mgmt.setProcessType("permanent");
			mgmt.setArgs("-i initconfig.txt");
			mgmt.setConnectString("nodeid=" + Convert.ToString(m_noOfConfiguredMgmt+1)+";host="+m_mgmHost+":" + m_mgmPort);
			this.m_noOfConfiguredMgmt++;
		}

		public void saveApi()
		{
			if(checkBoxReuse.Checked) 
			{
				for(;m_noOfConfiguredApi<m_nAPI;m_noOfConfiguredApi++)
				{
					String process="api." + Convert.ToString(m_noOfConfiguredApi+m_nMGM+m_nNDB+1);
					Process api=m_db.getProcessByName(process);
					textName.Text=process;
					api.setPath(textPath.Text.ToString());
					api.setArgs(textArgs.Text.ToString());
					api.setCwd(textCwd.Text.ToString());
					api.setEnv(textEnv.Text.ToString());
					api.setConnectString("nodeid=" + Convert.ToString(m_noOfConfiguredApi+m_nNDB+m_nMGM+1)+";host="+m_mgmHost+":" + m_mgmPort);
					api.setProcessType("permanent");
				}
 
			}	
			else
			{
				String process="api." + Convert.ToString(m_noOfConfiguredApi+m_nMGM+m_nNDB+1);
				Process api=m_db.getProcessByName(process);
				api.setPath(textPath.Text.ToString());
				api.setCwd(textCwd.Text.ToString());
				api.setEnv(textEnv.Text.ToString());
				api.setConnectString("nodeid=" + Convert.ToString(m_noOfConfiguredApi+m_nNDB+m_nMGM+1)+";host="+m_mgmHost+":" + m_mgmPort);
				api.setArgs(textArgs.Text.ToString());
				api.setProcessType("permanent");
				this.m_noOfConfiguredApi++;
			}
		}

		public void saveNdb()
		{
			
			if(checkBoxReuse.Checked) 
			{
				for(;m_noOfConfiguredNdb<m_nNDB;m_noOfConfiguredNdb++)
				{
					String process="ndb." + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1);
					Process ndb=m_db.getProcessByName(process);
					ndb.setConnectString("nodeid=" + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1)+";host="+m_mgmHost+":" + m_mgmPort);
					ndb.setPath(textPath.Text.ToString());
					ndb.setArgs(textArgs.Text.ToString());
					ndb.setEnv(textEnv.Text.ToString());
					ndb.setCwd(textCwd.Text.ToString());
					ndb.setProcessType("permanent");	
				}
				checkBoxReuse.Checked=false;
				return;
			}	
			else
			{
				String process="ndb." + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1);
				Process ndb=m_db.getProcessByName(process);
				ndb.setConnectString("nodeid=" + Convert.ToString(m_noOfConfiguredNdb+m_nMGM+1)+";host="+m_mgmHost+":" + m_mgmPort);
				ndb.setPath(textPath.Text.ToString());
				ndb.setCwd(textCwd.Text.ToString());
				ndb.setArgs(textArgs.Text.ToString());
				ndb.setEnv(textEnv.Text.ToString());
				ndb.setProcessType("permanent");
				m_noOfConfiguredNdb++;
			}
			
		}


		private void panel5_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			nextEnabled=false;
			buttonSave.Enabled=true;
			checkBoxReuse.Visible=false;
			refreshLook();
			configureMgmt();
		}

		private void buttonSave_Click(object sender, System.EventArgs e)
		{
			Process p = m_db.getProcessByName(textName.Text.ToString());

			if(textOther.Text.ToString().Equals("")) 
			{
				if(textName.Text.StartsWith("mgm"))  
				{
					MessageBox.Show("You have to specify a port.","Warning",MessageBoxButtons.OK);
					return;
				}
				if(textName.Text.StartsWith("ndb"))  
				{
					MessageBox.Show("You have to specify a filesystem path.","Warning",MessageBoxButtons.OK);
					return;
				}

			}
			
			if(textPath.Text.ToString().Equals("")) 
			{
				if(textName.Text.StartsWith("mgm"))  
				{
					MessageBox.Show("You have to specify the path to the mgmtsrvr.","Warning",MessageBoxButtons.OK);
					return;
				}
				if(textName.Text.StartsWith("ndb"))  
				{
					MessageBox.Show("You have to specify the path to ndb.","Warning",MessageBoxButtons.OK);
					return;
				}
			}
			
			if(textArgs.Text.ToString().Equals("")) 
			{
				if(textName.Text.StartsWith("mgm"))  
				{
					MessageBox.Show("You have to specify the arguments to the mgmtsrvr.","Warning",MessageBoxButtons.OK);
					return;
				}
			}
			
			if(textCwd.Text.ToString().Equals("")) 
			{
				if(textCwd.Text.StartsWith("mgm")) 
				{
					MessageBox.Show("You have to specify the current working directory for the mgmtsrvr.","Warning",MessageBoxButtons.OK);
					return;
				}
			}


			/*
			 * INPUT IS FINE AT THIS POINT
			 * Everything needed for respective process is ok
			 * */

			if(textName.Text.StartsWith("mgm")) 
			{
				//MessageBox.Show(textOther.Text.ToString());
				saveMgm();
			
			}

			if(textName.Text.StartsWith("ndb")) 
			{
				saveNdb();
		
			}

			if(textName.Text.StartsWith("api")) 
			{
				saveApi();
		
			}

			if(m_noOfConfiguredMgmt < m_nMGM) 
			{
				//load another Mgmt
				labelTitle.Text="Mgmtsrvr configuration";
				configureMgmt();
			} 
			else
			{
				m_bMgmt=true;
			}

			if(m_bMgmt) 
			{
				labelTitle.Text="NDB node configuration";
				if(m_noOfConfiguredNdb < m_nNDB) 
				{
					configureNdb();
				}
				else
					m_bNdb=true;
			}

			if(m_bNdb && m_bMgmt) 
			{
				labelTitle.Text="API node configuration";
				if(m_noOfConfiguredApi < m_nAPI)
					configureApi();
				else 
				{
					nextEnabled=true;
					buttonSave.Enabled=false;
					refreshLook();
				}
			}
			
		}

		private void listBoxComputers_SelectedIndexChanged(object sender, System.EventArgs e)
		{
		
		}

		private void panel1_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			updateComputers();
		}

		private void radioYes_CheckedChanged(object sender, System.EventArgs e)
		{
			if(radioYes.Checked==true) 
			{
				radioNo.Checked=false;
			}
			if(radioYes.Checked==false) 
			{
				radioNo.Checked=true;
			}

		}

		private void radioNo_CheckedChanged(object sender, System.EventArgs e)
		{
			if(radioNo.Checked==true) 
			{
				radioYes.Checked=false;
			}
			if(radioNo.Checked==false) 
			{
				radioYes.Checked=true;
			}		
		}

		private void radioStartYes_CheckedChanged(object sender, System.EventArgs e)
		{
		
			if(radioStartYes.Checked==true) 
			{
				radioStartNo.Checked=false;
			}
			if(radioStartYes.Checked==false) 
			{
				radioStartNo.Checked=true;
			}
			finishEnabled=true;
			refreshLook();
		}

		private void radioStartNo_CheckedChanged(object sender, System.EventArgs e)
		{
			if(radioStartNo.Checked==true) 
			{
				radioStartYes.Checked=false;
			}
			if(radioStartNo.Checked==false) 
			{
				radioStartYes.Checked=true;
			}
			finishEnabled=true;
			refreshLook();
		}

		

		
		public void startDatabase()
		{
			startDatabaseDlg x = new startDatabaseDlg(this.m_db);
			
		
			x.ShowDialog();
			

		}

		
		public void generateInitConfig()
		{
			MessageBox.Show("Generate initconfig.txt");
		}

		private void label11_Click(object sender, System.EventArgs e)
		{
		
		}

		private void textOwner_TextChanged(object sender, System.EventArgs e)
		{
			if(textDbName.TextLength > 0 && textOwner.TextLength > 0)
				nextEnabled=true;
			else
				nextEnabled=false;

			refreshLook();
		}

		private void panel2_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			textOwner.Text=System.Environment.UserName;
			this.Validate();
			if(textDbName.TextLength > 0 && textOwner.TextLength>0)
			{
				nextEnabled=true;
			}
			else
			{
				nextEnabled=false;
			}
			refreshLook();
		}

		private void textPath_TextChanged(object sender, System.EventArgs e)
		{
			try 
			{

			}
			catch (Exception exc)
			{
				MessageBox.Show(exc.ToString());
			}
		}

		private void panel2_Validating(object sender, System.ComponentModel.CancelEventArgs e)
		{
			if(textOwner.TextLength>0 && textDbName.TextLength > 0)
				nextEnabled=true;
			else
				nextEnabled=false;
		}

		



	
	}
}
