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
	/// Summary description for ComputerRemoveDialog.
	/// </summary>
	public class ComputerRemoveDialog : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.ComboBox comboComputer;
		private System.Windows.Forms.Button btnCancel;
		private System.Windows.Forms.Button btnRemove;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		private ComputerMgmt mgmt;
		
		public ComputerRemoveDialog(ComputerMgmt mgmt)
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
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(ComputerRemoveDialog));
			this.label1 = new System.Windows.Forms.Label();
			this.comboComputer = new System.Windows.Forms.ComboBox();
			this.btnCancel = new System.Windows.Forms.Button();
			this.btnRemove = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AccessibleDescription = ((string)(resources.GetObject("label1.AccessibleDescription")));
			this.label1.AccessibleName = ((string)(resources.GetObject("label1.AccessibleName")));
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(resources.GetObject("label1.Anchor")));
			this.label1.AutoSize = ((bool)(resources.GetObject("label1.AutoSize")));
			this.label1.Dock = ((System.Windows.Forms.DockStyle)(resources.GetObject("label1.Dock")));
			this.label1.Enabled = ((bool)(resources.GetObject("label1.Enabled")));
			this.label1.Font = ((System.Drawing.Font)(resources.GetObject("label1.Font")));
			this.label1.Image = ((System.Drawing.Image)(resources.GetObject("label1.Image")));
			this.label1.ImageAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("label1.ImageAlign")));
			this.label1.ImageIndex = ((int)(resources.GetObject("label1.ImageIndex")));
			this.label1.ImeMode = ((System.Windows.Forms.ImeMode)(resources.GetObject("label1.ImeMode")));
			this.label1.Location = ((System.Drawing.Point)(resources.GetObject("label1.Location")));
			this.label1.Name = "label1";
			this.label1.RightToLeft = ((System.Windows.Forms.RightToLeft)(resources.GetObject("label1.RightToLeft")));
			this.label1.Size = ((System.Drawing.Size)(resources.GetObject("label1.Size")));
			this.label1.TabIndex = ((int)(resources.GetObject("label1.TabIndex")));
			this.label1.Text = resources.GetString("label1.Text");
			this.label1.TextAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("label1.TextAlign")));
			this.label1.Visible = ((bool)(resources.GetObject("label1.Visible")));
			// 
			// comboComputer
			// 
			this.comboComputer.AccessibleDescription = ((string)(resources.GetObject("comboComputer.AccessibleDescription")));
			this.comboComputer.AccessibleName = ((string)(resources.GetObject("comboComputer.AccessibleName")));
			this.comboComputer.Anchor = ((System.Windows.Forms.AnchorStyles)(resources.GetObject("comboComputer.Anchor")));
			this.comboComputer.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("comboComputer.BackgroundImage")));
			this.comboComputer.Dock = ((System.Windows.Forms.DockStyle)(resources.GetObject("comboComputer.Dock")));
			this.comboComputer.Enabled = ((bool)(resources.GetObject("comboComputer.Enabled")));
			this.comboComputer.Font = ((System.Drawing.Font)(resources.GetObject("comboComputer.Font")));
			this.comboComputer.ImeMode = ((System.Windows.Forms.ImeMode)(resources.GetObject("comboComputer.ImeMode")));
			this.comboComputer.IntegralHeight = ((bool)(resources.GetObject("comboComputer.IntegralHeight")));
			this.comboComputer.ItemHeight = ((int)(resources.GetObject("comboComputer.ItemHeight")));
			this.comboComputer.Location = ((System.Drawing.Point)(resources.GetObject("comboComputer.Location")));
			this.comboComputer.MaxDropDownItems = ((int)(resources.GetObject("comboComputer.MaxDropDownItems")));
			this.comboComputer.MaxLength = ((int)(resources.GetObject("comboComputer.MaxLength")));
			this.comboComputer.Name = "comboComputer";
			this.comboComputer.RightToLeft = ((System.Windows.Forms.RightToLeft)(resources.GetObject("comboComputer.RightToLeft")));
			this.comboComputer.Size = ((System.Drawing.Size)(resources.GetObject("comboComputer.Size")));
			this.comboComputer.Sorted = true;
			this.comboComputer.TabIndex = ((int)(resources.GetObject("comboComputer.TabIndex")));
			this.comboComputer.Text = resources.GetString("comboComputer.Text");
			this.comboComputer.Visible = ((bool)(resources.GetObject("comboComputer.Visible")));
			this.comboComputer.SelectedIndexChanged += new System.EventHandler(this.comboComputer_SelectedIndexChanged);
			// 
			// btnCancel
			// 
			this.btnCancel.AccessibleDescription = ((string)(resources.GetObject("btnCancel.AccessibleDescription")));
			this.btnCancel.AccessibleName = ((string)(resources.GetObject("btnCancel.AccessibleName")));
			this.btnCancel.Anchor = ((System.Windows.Forms.AnchorStyles)(resources.GetObject("btnCancel.Anchor")));
			this.btnCancel.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("btnCancel.BackgroundImage")));
			this.btnCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.btnCancel.Dock = ((System.Windows.Forms.DockStyle)(resources.GetObject("btnCancel.Dock")));
			this.btnCancel.Enabled = ((bool)(resources.GetObject("btnCancel.Enabled")));
			this.btnCancel.FlatStyle = ((System.Windows.Forms.FlatStyle)(resources.GetObject("btnCancel.FlatStyle")));
			this.btnCancel.Font = ((System.Drawing.Font)(resources.GetObject("btnCancel.Font")));
			this.btnCancel.Image = ((System.Drawing.Image)(resources.GetObject("btnCancel.Image")));
			this.btnCancel.ImageAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("btnCancel.ImageAlign")));
			this.btnCancel.ImageIndex = ((int)(resources.GetObject("btnCancel.ImageIndex")));
			this.btnCancel.ImeMode = ((System.Windows.Forms.ImeMode)(resources.GetObject("btnCancel.ImeMode")));
			this.btnCancel.Location = ((System.Drawing.Point)(resources.GetObject("btnCancel.Location")));
			this.btnCancel.Name = "btnCancel";
			this.btnCancel.RightToLeft = ((System.Windows.Forms.RightToLeft)(resources.GetObject("btnCancel.RightToLeft")));
			this.btnCancel.Size = ((System.Drawing.Size)(resources.GetObject("btnCancel.Size")));
			this.btnCancel.TabIndex = ((int)(resources.GetObject("btnCancel.TabIndex")));
			this.btnCancel.Text = resources.GetString("btnCancel.Text");
			this.btnCancel.TextAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("btnCancel.TextAlign")));
			this.btnCancel.Visible = ((bool)(resources.GetObject("btnCancel.Visible")));
			this.btnCancel.Click += new System.EventHandler(this.btnCancel_Click);
			// 
			// btnRemove
			// 
			this.btnRemove.AccessibleDescription = ((string)(resources.GetObject("btnRemove.AccessibleDescription")));
			this.btnRemove.AccessibleName = ((string)(resources.GetObject("btnRemove.AccessibleName")));
			this.btnRemove.Anchor = ((System.Windows.Forms.AnchorStyles)(resources.GetObject("btnRemove.Anchor")));
			this.btnRemove.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("btnRemove.BackgroundImage")));
			this.btnRemove.Dock = ((System.Windows.Forms.DockStyle)(resources.GetObject("btnRemove.Dock")));
			this.btnRemove.Enabled = ((bool)(resources.GetObject("btnRemove.Enabled")));
			this.btnRemove.FlatStyle = ((System.Windows.Forms.FlatStyle)(resources.GetObject("btnRemove.FlatStyle")));
			this.btnRemove.Font = ((System.Drawing.Font)(resources.GetObject("btnRemove.Font")));
			this.btnRemove.Image = ((System.Drawing.Image)(resources.GetObject("btnRemove.Image")));
			this.btnRemove.ImageAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("btnRemove.ImageAlign")));
			this.btnRemove.ImageIndex = ((int)(resources.GetObject("btnRemove.ImageIndex")));
			this.btnRemove.ImeMode = ((System.Windows.Forms.ImeMode)(resources.GetObject("btnRemove.ImeMode")));
			this.btnRemove.Location = ((System.Drawing.Point)(resources.GetObject("btnRemove.Location")));
			this.btnRemove.Name = "btnRemove";
			this.btnRemove.RightToLeft = ((System.Windows.Forms.RightToLeft)(resources.GetObject("btnRemove.RightToLeft")));
			this.btnRemove.Size = ((System.Drawing.Size)(resources.GetObject("btnRemove.Size")));
			this.btnRemove.TabIndex = ((int)(resources.GetObject("btnRemove.TabIndex")));
			this.btnRemove.Text = resources.GetString("btnRemove.Text");
			this.btnRemove.TextAlign = ((System.Drawing.ContentAlignment)(resources.GetObject("btnRemove.TextAlign")));
			this.btnRemove.Visible = ((bool)(resources.GetObject("btnRemove.Visible")));
			this.btnRemove.Click += new System.EventHandler(this.btnRemove_Click);
			// 
			// ComputerRemoveDialog
			// 
			this.AcceptButton = this.btnRemove;
			this.AccessibleDescription = ((string)(resources.GetObject("$this.AccessibleDescription")));
			this.AccessibleName = ((string)(resources.GetObject("$this.AccessibleName")));
			this.Anchor = ((System.Windows.Forms.AnchorStyles)(resources.GetObject("$this.Anchor")));
			this.AutoScaleBaseSize = ((System.Drawing.Size)(resources.GetObject("$this.AutoScaleBaseSize")));
			this.AutoScroll = ((bool)(resources.GetObject("$this.AutoScroll")));
			this.AutoScrollMargin = ((System.Drawing.Size)(resources.GetObject("$this.AutoScrollMargin")));
			this.AutoScrollMinSize = ((System.Drawing.Size)(resources.GetObject("$this.AutoScrollMinSize")));
			this.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("$this.BackgroundImage")));
			this.CancelButton = this.btnCancel;
			this.ClientSize = ((System.Drawing.Size)(resources.GetObject("$this.ClientSize")));
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.btnRemove,
																		  this.btnCancel,
																		  this.comboComputer,
																		  this.label1});
			this.Dock = ((System.Windows.Forms.DockStyle)(resources.GetObject("$this.Dock")));
			this.Enabled = ((bool)(resources.GetObject("$this.Enabled")));
			this.Font = ((System.Drawing.Font)(resources.GetObject("$this.Font")));
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.ImeMode = ((System.Windows.Forms.ImeMode)(resources.GetObject("$this.ImeMode")));
			this.Location = ((System.Drawing.Point)(resources.GetObject("$this.Location")));
			this.MaximizeBox = false;
			this.MaximumSize = ((System.Drawing.Size)(resources.GetObject("$this.MaximumSize")));
			this.MinimizeBox = false;
			this.MinimumSize = ((System.Drawing.Size)(resources.GetObject("$this.MinimumSize")));
			this.Name = "ComputerRemoveDialog";
			this.RightToLeft = ((System.Windows.Forms.RightToLeft)(resources.GetObject("$this.RightToLeft")));
			this.StartPosition = ((System.Windows.Forms.FormStartPosition)(resources.GetObject("$this.StartPosition")));
			this.Text = resources.GetString("$this.Text");
			this.Visible = ((bool)(resources.GetObject("$this.Visible")));
			this.Load += new System.EventHandler(this.ComputerRemoveDialog_Load);
			this.ResumeLayout(false);

		}
		#endregion

		private void btnRemove_Click(object sender, System.EventArgs e)
		{
			mgmt.RemoveComputer(comboComputer.SelectedItem.ToString());
			this.Dispose();
		}

		private void ComputerRemoveDialog_Load(object sender, System.EventArgs e)
		{
			ArrayList list = mgmt.getComputerCollection();
			foreach (Computer computer in list) 
			{
				comboComputer.Items.Add(computer.getName());
			}
		}

		private void btnCancel_Click(object sender, System.EventArgs e)
		{
			this.Close();
			this.Dispose();
		}

		private void comboComputer_SelectedIndexChanged(object sender, System.EventArgs e)
		{
		}


	}
}
