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

namespace NDB_CPC
{
	/// <summary>
	/// Summary description for ProcessDefineDialog.
	/// </summary>
	public class ProcessDefineDialog : System.Windows.Forms.Form
	{
		private System.Windows.Forms.ComboBox comboComputer;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.Label label7;
		private System.Windows.Forms.Label label8;
		private System.Windows.Forms.Label label9;
		private System.Windows.Forms.TextBox textProcessName;
		private System.Windows.Forms.TextBox textProcessGroup;
		private System.Windows.Forms.TextBox textProcessEnv;
		private System.Windows.Forms.TextBox textProcessPath;
		private System.Windows.Forms.TextBox textProcessArgs;
		private System.Windows.Forms.TextBox textProcessCWD;
		private System.Windows.Forms.TextBox textProcessOwner;
		private System.Windows.Forms.ComboBox comboType;
		private System.Windows.Forms.Label label10;
		private System.Windows.Forms.Label label11;
		private System.Windows.Forms.Label label12;
		private System.Windows.Forms.Label label13;
		private System.Windows.Forms.Label label15;
		private System.Windows.Forms.Label label16;
		private System.Windows.Forms.Label label14;
		private System.Windows.Forms.Label label17;
		private System.Windows.Forms.Label label18;
		private System.Windows.Forms.Button btnAdd;
		private System.Windows.Forms.Button btnCancel;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		private ComputerMgmt c_mgmt;
		private string m_selComputer;
		public ProcessDefineDialog(ComputerMgmt mgmt, string computer)
		{
		
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
			m_selComputer =computer; //the selected computer in the TreeView
			c_mgmt=mgmt;
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
			this.comboComputer = new System.Windows.Forms.ComboBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.label3 = new System.Windows.Forms.Label();
			this.label4 = new System.Windows.Forms.Label();
			this.label5 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.label7 = new System.Windows.Forms.Label();
			this.label8 = new System.Windows.Forms.Label();
			this.label9 = new System.Windows.Forms.Label();
			this.textProcessName = new System.Windows.Forms.TextBox();
			this.textProcessGroup = new System.Windows.Forms.TextBox();
			this.textProcessEnv = new System.Windows.Forms.TextBox();
			this.textProcessPath = new System.Windows.Forms.TextBox();
			this.textProcessArgs = new System.Windows.Forms.TextBox();
			this.textProcessCWD = new System.Windows.Forms.TextBox();
			this.textProcessOwner = new System.Windows.Forms.TextBox();
			this.comboType = new System.Windows.Forms.ComboBox();
			this.label10 = new System.Windows.Forms.Label();
			this.label11 = new System.Windows.Forms.Label();
			this.label12 = new System.Windows.Forms.Label();
			this.label13 = new System.Windows.Forms.Label();
			this.label15 = new System.Windows.Forms.Label();
			this.label16 = new System.Windows.Forms.Label();
			this.label14 = new System.Windows.Forms.Label();
			this.label17 = new System.Windows.Forms.Label();
			this.label18 = new System.Windows.Forms.Label();
			this.btnAdd = new System.Windows.Forms.Button();
			this.btnCancel = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// comboComputer
			// 
			this.comboComputer.ItemHeight = 13;
			this.comboComputer.Location = new System.Drawing.Point(152, 24);
			this.comboComputer.Name = "comboComputer";
			this.comboComputer.Size = new System.Drawing.Size(112, 21);
			this.comboComputer.TabIndex = 0;
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(24, 24);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(64, 24);
			this.label1.TabIndex = 1;
			this.label1.Text = "Computer:";
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(24, 48);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(88, 24);
			this.label2.TabIndex = 2;
			this.label2.Text = "Process name:";
			// 
			// label3
			// 
			this.label3.Location = new System.Drawing.Point(24, 72);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(88, 24);
			this.label3.TabIndex = 3;
			this.label3.Text = "Group:";
			// 
			// label4
			// 
			this.label4.Location = new System.Drawing.Point(24, 96);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(88, 24);
			this.label4.TabIndex = 4;
			this.label4.Text = "Env. variables:";
			// 
			// label5
			// 
			this.label5.Location = new System.Drawing.Point(24, 120);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(88, 24);
			this.label5.TabIndex = 5;
			this.label5.Text = "Path to binary:";
			// 
			// label6
			// 
			this.label6.Location = new System.Drawing.Point(24, 144);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(112, 24);
			this.label6.TabIndex = 6;
			this.label6.Text = "Arguments to binary:";
			// 
			// label7
			// 
			this.label7.Location = new System.Drawing.Point(24, 168);
			this.label7.Name = "label7";
			this.label7.Size = new System.Drawing.Size(112, 24);
			this.label7.TabIndex = 7;
			this.label7.Text = "Type of process:";
			// 
			// label8
			// 
			this.label8.Location = new System.Drawing.Point(24, 192);
			this.label8.Name = "label8";
			this.label8.Size = new System.Drawing.Size(112, 24);
			this.label8.TabIndex = 8;
			this.label8.Text = "Current working dir.:";
			// 
			// label9
			// 
			this.label9.Location = new System.Drawing.Point(24, 216);
			this.label9.Name = "label9";
			this.label9.Size = new System.Drawing.Size(112, 24);
			this.label9.TabIndex = 9;
			this.label9.Text = "Owner:";
			// 
			// textProcessName
			// 
			this.textProcessName.Location = new System.Drawing.Point(152, 48);
			this.textProcessName.Name = "textProcessName";
			this.textProcessName.Size = new System.Drawing.Size(112, 20);
			this.textProcessName.TabIndex = 1;
			this.textProcessName.Text = "";
			// 
			// textProcessGroup
			// 
			this.textProcessGroup.Location = new System.Drawing.Point(152, 72);
			this.textProcessGroup.Name = "textProcessGroup";
			this.textProcessGroup.Size = new System.Drawing.Size(112, 20);
			this.textProcessGroup.TabIndex = 2;
			this.textProcessGroup.Text = "";
			// 
			// textProcessEnv
			// 
			this.textProcessEnv.Location = new System.Drawing.Point(152, 96);
			this.textProcessEnv.Name = "textProcessEnv";
			this.textProcessEnv.Size = new System.Drawing.Size(112, 20);
			this.textProcessEnv.TabIndex = 3;
			this.textProcessEnv.Text = "";
			// 
			// textProcessPath
			// 
			this.textProcessPath.Location = new System.Drawing.Point(152, 120);
			this.textProcessPath.Name = "textProcessPath";
			this.textProcessPath.Size = new System.Drawing.Size(112, 20);
			this.textProcessPath.TabIndex = 4;
			this.textProcessPath.Text = "";
			// 
			// textProcessArgs
			// 
			this.textProcessArgs.Location = new System.Drawing.Point(152, 144);
			this.textProcessArgs.Name = "textProcessArgs";
			this.textProcessArgs.Size = new System.Drawing.Size(112, 20);
			this.textProcessArgs.TabIndex = 5;
			this.textProcessArgs.Text = "";
			// 
			// textProcessCWD
			// 
			this.textProcessCWD.Location = new System.Drawing.Point(152, 192);
			this.textProcessCWD.Name = "textProcessCWD";
			this.textProcessCWD.Size = new System.Drawing.Size(112, 20);
			this.textProcessCWD.TabIndex = 7;
			this.textProcessCWD.Text = "";
			// 
			// textProcessOwner
			// 
			this.textProcessOwner.Location = new System.Drawing.Point(152, 216);
			this.textProcessOwner.Name = "textProcessOwner";
			this.textProcessOwner.Size = new System.Drawing.Size(112, 20);
			this.textProcessOwner.TabIndex = 8;
			this.textProcessOwner.Text = "";
			// 
			// comboType
			// 
			this.comboType.ItemHeight = 13;
			this.comboType.Items.AddRange(new object[] {
														   "Permanent",
														   "Interactive"});
			this.comboType.Location = new System.Drawing.Point(152, 168);
			this.comboType.Name = "comboType";
			this.comboType.Size = new System.Drawing.Size(112, 21);
			this.comboType.TabIndex = 6;
			// 
			// label10
			// 
			this.label10.Location = new System.Drawing.Point(272, 32);
			this.label10.Name = "label10";
			this.label10.Size = new System.Drawing.Size(88, 16);
			this.label10.TabIndex = 19;
			this.label10.Text = "(Mandatory)";
			// 
			// label11
			// 
			this.label11.Location = new System.Drawing.Point(272, 56);
			this.label11.Name = "label11";
			this.label11.Size = new System.Drawing.Size(88, 16);
			this.label11.TabIndex = 20;
			this.label11.Text = "(Mandatory)";
			// 
			// label12
			// 
			this.label12.Location = new System.Drawing.Point(272, 80);
			this.label12.Name = "label12";
			this.label12.Size = new System.Drawing.Size(88, 16);
			this.label12.TabIndex = 21;
			this.label12.Text = "(Mandatory)";
			// 
			// label13
			// 
			this.label13.Location = new System.Drawing.Point(272, 127);
			this.label13.Name = "label13";
			this.label13.Size = new System.Drawing.Size(88, 16);
			this.label13.TabIndex = 22;
			this.label13.Text = "(Mandatory)";
			// 
			// label15
			// 
			this.label15.Location = new System.Drawing.Point(272, 176);
			this.label15.Name = "label15";
			this.label15.Size = new System.Drawing.Size(88, 16);
			this.label15.TabIndex = 24;
			this.label15.Text = "(Mandatory)";
			// 
			// label16
			// 
			this.label16.Location = new System.Drawing.Point(272, 200);
			this.label16.Name = "label16";
			this.label16.Size = new System.Drawing.Size(88, 16);
			this.label16.TabIndex = 25;
			this.label16.Text = "(Mandatory)";
			// 
			// label14
			// 
			this.label14.Location = new System.Drawing.Point(272, 224);
			this.label14.Name = "label14";
			this.label14.Size = new System.Drawing.Size(88, 16);
			this.label14.TabIndex = 26;
			this.label14.Text = "(Mandatory)";
			// 
			// label17
			// 
			this.label17.Location = new System.Drawing.Point(272, 104);
			this.label17.Name = "label17";
			this.label17.Size = new System.Drawing.Size(88, 16);
			this.label17.TabIndex = 27;
			this.label17.Text = "(Optional)";
			// 
			// label18
			// 
			this.label18.Location = new System.Drawing.Point(272, 152);
			this.label18.Name = "label18";
			this.label18.Size = new System.Drawing.Size(88, 16);
			this.label18.TabIndex = 28;
			this.label18.Text = "(Optional)";
			// 
			// btnAdd
			// 
			this.btnAdd.Location = new System.Drawing.Point(288, 248);
			this.btnAdd.Name = "btnAdd";
			this.btnAdd.TabIndex = 9;
			this.btnAdd.Text = "Define...";
			this.btnAdd.Click += new System.EventHandler(this.btnAdd_Click);
			// 
			// btnCancel
			// 
			this.btnCancel.Location = new System.Drawing.Point(152, 248);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.TabIndex = 10;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
			// 
			// ProcessDefineDialog
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(370, 279);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.btnCancel,
																		  this.btnAdd,
																		  this.label18,
																		  this.label17,
																		  this.label14,
																		  this.label16,
																		  this.label15,
																		  this.label13,
																		  this.label12,
																		  this.label11,
																		  this.label10,
																		  this.comboType,
																		  this.textProcessOwner,
																		  this.textProcessCWD,
																		  this.textProcessArgs,
																		  this.textProcessPath,
																		  this.textProcessEnv,
																		  this.textProcessGroup,
																		  this.textProcessName,
																		  this.label9,
																		  this.label8,
																		  this.label7,
																		  this.label6,
																		  this.label5,
																		  this.label4,
																		  this.label3,
																		  this.label2,
																		  this.label1,
																		  this.comboComputer});
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ProcessDefineDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Define Process";
			this.Load += new System.EventHandler(this.ProcessDefineDialog_Load);
			this.ResumeLayout(false);

		}
		#endregion

		private void btnCancel_Click(object sender, System.EventArgs e)
		{
			this.Dispose();
			this.Close();		
		}

		private void btnAdd_Click(object sender, System.EventArgs e)
		{
			//TODO: ERROR CHECK
		
			Computer c;
			c=c_mgmt.getComputer(this.m_selComputer);
			
			c.addProcess(new Process(this.textProcessName.Text.ToString(),
									this.textProcessOwner.Text.ToString(),
									this.textProcessGroup.Text.ToString(),
									c));
			this.Close();
			this.Dispose();
		}

		private void ProcessDefineDialog_Load(object sender, System.EventArgs e)
		{
			comboType.SelectedIndex=0;
			ArrayList list = c_mgmt.getComputerCollection();
			int i=0, selIndex=0;
			foreach(Computer computer in list) 
			{
				this.comboComputer.Items.Add(computer.getName());
				if(computer.getName().Equals(m_selComputer))
					selIndex=i;
				i++;
			}
			comboComputer.SelectedIndex=selIndex;
		}

		
	}
}
