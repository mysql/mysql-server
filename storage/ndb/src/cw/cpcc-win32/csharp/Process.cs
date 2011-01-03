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

namespace NDB_CPC
{
	/// <summary>
	/// Summary description for Process.
	/// </summary>
	public class Process 
	{
		public enum Status {Running, Stopped, Unknown}
		private string m_id;
		protected string m_name;
		private Status m_status;
		private Computer m_computer;
		private string m_owner;
		private string m_cwd;
		private string m_type;
		private string m_path;
		private string m_other;
		private string m_args;
		private string m_env;
		private string m_database;
		private string m_connectString;
		private bool m_defined;
		public Process( string name, 
					string owner, string database,
					Computer computer)
		{
			m_name=name;
			m_owner=owner;
			m_computer=computer;
			m_status=Status.Unknown;
			m_database=database;
			m_defined=false;
			m_path="";
			m_cwd="";
			m_args="";
			m_other="";
		}
		public Process()
		{
			
		}
		public Process(string id)
		{
			m_id=id;
		}

		public Process( string name, 
					 string database,
			Computer computer)
		{
			m_name=name;
			m_computer=computer;
			m_status=Status.Unknown;
			m_database=database;
			m_defined=false;
		}

		public Process( string name, 
			Computer computer)
		{
			m_name=name;
			m_computer=computer;
			m_status=Status.Unknown;
			m_defined=false;
		}


		public string getStatusString() 
		{
			if(m_status.Equals(Status.Running))
				return "Running";
			if(m_status.Equals(Status.Stopped))
				return "Stopped";
			return "Unknown";
		}

		public Computer getComputer() {return m_computer;}
		public string getName() {return m_name;}
		public string getDatabase() {return m_database;}
		public string getOwner() {return m_owner;}
		public string getId() {return m_id;}
		public void setId(string id) {m_id=id;}
	
		public void setCwd(string cwd) {m_cwd=cwd;}
		public void setPath(string path) {m_path=path;}
		public void setArgs(string args) {m_args=args;}
		public void setOther(string other) {m_other=other;}
		public void setEnv(string env) {m_env=env;}
		public void setName(string name) {m_name=name;}
		public void setOwner(string owner) {m_owner=owner;}
		public void setDatabase(string db) {m_database=db;}
		public void setComputer(Computer c) {m_computer=c;}
		

		public string getCwd() {return m_cwd;}
		public string getPath() {return m_path;}
		public string getArgs() {return m_args;}
		public string getOther() {return m_other;}
		public string getEnv() {return m_env;}

		public bool isDefined() {return m_defined;}
		public void setDefined(bool defined) 
		{
			m_defined=defined;
		}

		public Status getStatus() 
		{
			return m_status;
		}

		public void setConnectString(string cs)
		{
			m_connectString=cs;
		}

		public string getConnectString()
		{
			return m_connectString;
		}
		public void setStatus(Status status) 
		{
			m_status=status;
		}


		public void setProcessType(string type)
		{
			 m_type=type;
		}
		public string getProcessType()
		{
			return m_type;
		}
		
	}
}
