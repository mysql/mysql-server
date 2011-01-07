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
	/// Summary description for ComputerAddDialog.
	/// </summary>
	public class ComputerAddDialog : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox textboxComputerName;
		private System.Windows.Forms.Button btnAdd;
		private System.Windows.Forms.Button btnCancel;
		private System.Windows.Forms.Label label2;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		private System.Windows.Forms.Label label6;
		private System.Windows.Forms.CheckBox checkBoxDefault;
		private System.Windows.Forms.TextBox textBoxPort;

		private ComputerMgmt mgmt;
		public ComputerAddDialog(ComputerMgmt mgmt)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
			this.mgmt=mgmt;
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
			this.textboxComputerName = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.btnAdd = new System.Windows.Forms.Button();
			this.btnCancel = new System.Windows.Forms.Button();
			this.label2 = new System.Windows.Forms.Label();
			this.label6 = new System.Windows.Forms.Label();
			this.textBoxPort = new System.Windows.Forms.TextBox();
			this.checkBoxDefault = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// textboxComputerName
			// 
			this.textboxComputerName.Location = new System.Drawing.Point(128, 16);
			this.textboxComputerName.Name = "textboxComputerName";
			this.textboxComputerName.Size = new System.Drawing.Size(136, 20);
			this.textboxComputerName.TabIndex = 0;
			this.textboxComputerName.Text = "";
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(40, 16);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(88, 23);
			this.label1.TabIndex = 1;
			this.label1.Text = "Computer name:";
			this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			// 
			// btnAdd
			// 
			this.btnAdd.Location = new System.Drawing.Point(112, 128);
			this.btnAdd.Name = "btnAdd";
			this.btnAdd.Size = new System.Drawing.Size(80, 24);
			this.btnAdd.TabIndex = 4;
			this.btnAdd.Text = "Add";
			this.btnAdd.Click += new System.EventHandler(this.btnAdd_Click);
			// 
			// btnCancel
			// 
			this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.btnCancel.Location = new System.Drawing.Point(200, 128);
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.Size = new System.Drawing.Size(80, 24);
			this.btnCancel.TabIndex = 5;
			this.btnCancel.Text = "Cancel";
			this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(128, 40);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(136, 16);
			this.label2.TabIndex = 4;
			this.label2.Text = "(e.g. Ndb01 or 10.0.1.1)";
			// 
			// label6
			// 
			this.label6.Location = new System.Drawing.Point(48, 64);
			this.label6.Name = "label6";
			this.label6.Size = new System.Drawing.Size(80, 24);
			this.label6.TabIndex = 9;
			this.label6.Text = "CPCd port:";
			this.label6.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			// 
			// textBoxPort
			// 
			this.textBoxPort.Enabled = false;
			this.textBoxPort.Location = new System.Drawing.Point(128, 64);
			this.textBoxPort.Name = "textBoxPort";
			this.textBoxPort.Size = new System.Drawing.Size(136, 20);
			this.textBoxPort.TabIndex = 2;
			this.textBoxPort.TabStop = false;
			this.textBoxPort.Text = "";
			// 
			// checkBoxDefault
			// 
			this.checkBoxDefault.Checked = true;
			this.checkBoxDefault.CheckState = System.Windows.Forms.CheckState.Checked;
			this.checkBoxDefault.Location = new System.Drawing.Point(96, 96);
			this.checkBoxDefault.Name = "checkBoxDefault";
			this.checkBoxDefault.Size = new System.Drawing.Size(168, 16);
			this.checkBoxDefault.TabIndex = 3;
			this.checkBoxDefault.Text = "Use default port (1234)?";
			this.checkBoxDefault.CheckedChanged += new System.EventHandler(this.checkBoxDefault_CheckedChanged);
			// 
			// ComputerAddDialog
			// 
			this.AcceptButton = this.btnAdd;
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.CancelButton = this.btnCancel;
			this.ClientSize = new System.Drawing.Size(298, 159);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.checkBoxDefault,
																		  this.label6,
																		  this.textBoxPort,
																		  this.label2,
																		  this.btnCancel,
																		  this.btnAdd,
																		  this.label1,
																		  this.textboxComputerName});
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ComputerAddDialog";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Add a computer";
			this.Load += new System.EventHandler(this.ComputerAddDialog_Load);
			this.ResumeLayout(false);

		}
		#endregion

		private void btnCancel_Click(object sender, System.EventArgs e)
		{
			this.Close();
			this.Dispose();
		}

		private void btnAdd_Click(object sender, System.EventArgs e)
		{
			int port;
			if(this.textboxComputerName.Text.Equals(""))
			{
				MessageBox.Show(this,"A computer must have an IP address or a host name","Warning!",MessageBoxButtons.OK);
				return;
			}
			if(this.checkBoxDefault.Checked) 
			{
				port=1234;
			}
			else
			{
				if(this.textBoxPort.Text.Equals("")) 
				{
					MessageBox.Show(this,"You must specify a port number!!!","Warning!",MessageBoxButtons.OK);
					return;
				}
				else
				{
					try 
					{
						port=Convert.ToInt32(this.textBoxPort.Text.ToString());

					}
					catch (Exception exception)
					{
						MessageBox.Show(this,"Port number must be numeric!!!","Warning!",MessageBoxButtons.OK);
						return;
					}
				}
			}

			if(mgmt.getComputer(this.textboxComputerName.Text)==null) 
			{
				mgmt.AddComputer(this.textboxComputerName.Text.ToString(),port);}
			else
			{
				MessageBox.Show("This computer does already exist!", "Add computer");
				return;
			}
			
			this.Dispose();
		}

		private void ComputerAddDialog_Load(object sender, System.EventArgs e)
		{
		
		}

		private void checkBoxDefault_CheckedChanged(object sender, System.EventArgs e)
		{
			if(checkBoxDefault.Checked)
				textBoxPort.Enabled=false;
			else
				textBoxPort.Enabled=true;
		}

		

		
	}
}
