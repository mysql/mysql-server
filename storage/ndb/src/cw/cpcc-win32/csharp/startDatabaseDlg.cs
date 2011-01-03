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
using NDB_CPC.simpleparser;

namespace NDB_CPC
{
	/// <summary>
	/// Summary description for startDatabase.
	/// </summary>
	public class startDatabaseDlg : System.Windows.Forms.Form
	{
		private System.Windows.Forms.TextBox textAction;
		private System.Windows.Forms.Label label1;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		private System.Windows.Forms.ProgressBar progressBar;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Button buttonGo;
		private Database m_db;
		public startDatabaseDlg(Database db)
		{
			
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			//
			// TODO: Add any constructor code after InitializeComponent call
			//
			m_db=db;
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
			this.textAction = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.progressBar = new System.Windows.Forms.ProgressBar();
			this.label2 = new System.Windows.Forms.Label();
			this.buttonGo = new System.Windows.Forms.Button();
			this.SuspendLayout();
			// 
			// textAction
			// 
			this.textAction.Location = new System.Drawing.Point(104, 40);
			this.textAction.Name = "textAction";
			this.textAction.ReadOnly = true;
			this.textAction.Size = new System.Drawing.Size(256, 20);
			this.textAction.TabIndex = 0;
			this.textAction.Text = "";
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(8, 40);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(96, 16);
			this.label1.TabIndex = 1;
			this.label1.Text = "Current activity:";
			this.label1.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			// 
			// progressBar
			// 
			this.progressBar.Location = new System.Drawing.Point(104, 88);
			this.progressBar.Name = "progressBar";
			this.progressBar.Size = new System.Drawing.Size(152, 16);
			this.progressBar.TabIndex = 2;
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(8, 88);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(96, 16);
			this.label2.TabIndex = 3;
			this.label2.Text = "Activity progress:";
			this.label2.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// buttonGo
			// 
			this.buttonGo.Location = new System.Drawing.Point(152, 136);
			this.buttonGo.Name = "buttonGo";
			this.buttonGo.Size = new System.Drawing.Size(96, 24);
			this.buttonGo.TabIndex = 4;
			this.buttonGo.Text = "Go!";
			this.buttonGo.Click += new System.EventHandler(this.buttonGo_Click);
			// 
			// startDatabaseDlg
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(378, 167);
			this.Controls.AddRange(new System.Windows.Forms.Control[] {
																		  this.buttonGo,
																		  this.label2,
																		  this.progressBar,
																		  this.label1,
																		  this.textAction});
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "startDatabaseDlg";
			this.Text = "Starting database";
			this.Load += new System.EventHandler(this.startDatabase_Load);
			this.Paint += new System.Windows.Forms.PaintEventHandler(this.startDatabase_Paint);
			this.ResumeLayout(false);

		}
		#endregion

		private void startDatabase_Load(object sender, System.EventArgs e)
		{
		
		}

		private void startDatabase_Paint(object sender, System.Windows.Forms.PaintEventArgs e)
		{
			
			
			
		}
		private void defineProcesses()
		{
			ArrayList processes = m_db.getProcesses();
			progressBar.Maximum = processes.Count;
			progressBar.Minimum = 0;
			
			int retry=0;
			//sc.connect("130.100.232.7");
			foreach (Process p in processes)
			{
				Computer comp;
				retry=0;
				//if(p.getName().StartsWith("ndb") || p.getName().StartsWith("mgm")) 
				//{
					textAction.Text="Defining process " + p.getName();
					textAction.Refresh();
					comp=p.getComputer();
					while(retry<10)
					{
						if(!comp.isConnected())
						{
							comp.connectToCpcd();
							
						}
						else
						{
							if(comp.defineProcess(p)<0) 
							{
								;
							}
							else
								break;
						}
						if(retry==9) 
						{	
							if(MessageBox.Show(this,"Failed to define process. Try again?","Warning!!!",MessageBoxButtons.YesNo)==DialogResult.Yes)
								retry=0;
						}
						retry++;
						//comp.undefineProcess(p);
					}
				//}
				progressBar.PerformStep();
			}
		}

		private void startProcesses()
		{
			
			ArrayList processes = m_db.getProcesses();
			progressBar.Maximum = processes.Count;
			progressBar.Minimum = 0;
			string start = "start process \n";

			int retry=0;
			//sc.connect("130.100.232.7");
			foreach (Process p in processes)
			{
				Computer comp;
				if((p.getName().StartsWith("ndb")) ||  (p.getName().StartsWith("mgm")))
				{
					textAction.Text="Starting process " + p.getName();
					textAction.Refresh();
					start = start + "id:" + p.getId() + "\n\n";
					comp=p.getComputer();
					while(retry<10)
					{
						if(!comp.isConnected())
						{
							comp.connectToCpcd();
						}
						else
						{
							if(comp.startProcess(p)<0) 
							{
								;
							}
							else
								break;
						}
						if(retry==9) 
						{	
							if(MessageBox.Show(this,"Failed to start process. Retry again?","Warning!!!",MessageBoxButtons.YesNo)==DialogResult.Yes)
								retry=0;
						}
						
						retry++;
					}
				}
				progressBar.PerformStep();
				
			}

		}

		private void buttonGo_Click(object sender, System.EventArgs e)
		{
			buttonGo.Enabled=false;
			progressBar.Step=1;
			defineProcesses();
			progressBar.Value=0;
			startProcesses();
				
		}

		
	}
}
