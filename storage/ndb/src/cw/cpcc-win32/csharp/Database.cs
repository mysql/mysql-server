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
	/// Summary description for Database.
	/// </summary>
	public class Database
	{
		public enum	Status {Disconnected=1,Connected=2, Unknown=3}
		private string m_name;
		private string m_owner;
		private int m_mgmtPort;
		private Status m_status;
		private ArrayList m_processes;
		public Database(string name)
		{	
			m_name=name;
			m_processes = new ArrayList();
		}
		public Database(string name, string owner)
		{	
			m_name=name;
			m_owner=owner;
			m_processes = new ArrayList();
		}
		public Database()
		{	
			m_processes = new ArrayList();
		}

		public string getName() 
		{
			return m_name;
		}
		
		public void setName(string name) 
		{
			m_name=name;
		}
		
		public void setMgmtPort(int port) 
		{
			m_mgmtPort=port;
		}

		public string getOwner() 
		{
			return m_owner;
		}
		
		public void setOwner(string name) 
		{
			m_owner=name;
		}


		public Status getStatus() 
		{
			return m_status;
		}

		public string getStatusString() 
		{
			if(m_status.Equals(Status.Connected))
				return "Connected";
			if(m_status.Equals(Status.Disconnected))
				return "Disconnected";
			if(m_status.Equals(Status.Unknown))
				return "Unknown";
			return "Unknown";
		}
		public void setStatus(Status status) 
		{
			m_status=status;
		}

		public void addProcess(Process process) 
		{
			/*if(check) 
			{
				if(m_processes==null)
					return;
				if(m_processes.Count>0) 
				{
					foreach (Process p in m_processes)
					{
						if(process.getId().Equals(p.getId()))
							return;
					}
				}
			}
			*/
			m_processes.Add(process);			
		}
		public void addProcessCheck(Process process) 
		{
	
			if(m_processes==null)
				return;
			if(m_processes.Count>0) 
			{
				foreach (Process p in m_processes)
				{
					if(process.getId().Equals(p.getId()))
						return;
				}
			}
			m_processes.Add(process);			
		}

		public Process getProcess(string id) 
		{
			foreach(Process process in m_processes)
			{
				if(process.getId().Equals(id))
					return process;
			}
			return null;
		}

		public Process getProcessByName(string name) 
		{
			foreach(Process process in m_processes)
			{
				if(process.getName().Equals(name))
					return process;
			}
			return null;
		}
		
		public void removeProcess( string processName)
		{
			Process p = this.getProcessByName(processName);
			m_processes.Remove(p);
		}

		public void removeAllProcesses() 
		{
			Computer c;
			foreach(Process p in m_processes)
			{
				c=p.getComputer();
				if(c.removeProcess(p.getName(),m_name).Equals(false)) 
				{
				
				}
			}
			m_processes.Clear();
		}

		public ArrayList getProcesses() 
		{
			return m_processes;
		}
	}
}
