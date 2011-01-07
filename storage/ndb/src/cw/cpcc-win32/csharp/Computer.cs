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
using System.IO;
using NDB_CPC.socketcomm;
using NDB_CPC.simpleparser;


namespace NDB_CPC
{
	/// <summary>
	/// Summary description for Computer.
	/// </summary>
	public class Computer
	{
		public enum	Status {Disconnected=1,Connected=2, Unknown=3}
		private string m_ip;
		private int m_cpcdPort;
		private string m_name;
		private Status m_status;
		private ArrayList m_processes;
		private SocketComm m_socket;
		public Computer(string name, int port)
		{
			m_name = name;
			m_status = Status.Disconnected;
			m_processes = new ArrayList();
			m_cpcdPort=port;
			m_socket = new SocketComm(m_name,m_cpcdPort);
		}

		public Computer(string name, string ip)
		{
			m_ip = ip;
			m_name = name;
			m_status = Status.Disconnected;
			m_processes = new ArrayList();
			m_cpcdPort=1234; //default port
			m_socket = new SocketComm(m_ip,m_cpcdPort);
		}

		public void connectToCpcd()
		{
			m_socket.doConnect();
		}

		private bool sendMessage(string str)
		{	
			return m_socket.writeMessage(str);
			
		}
		
		public string getName() {return m_name;}
		public string getIp() {return m_ip;}
		public ArrayList getProcesses() 
		{ 
			if(m_processes.Count>0)
				return m_processes;	
			else
				return null;
		}
		public string getStatusString() 
		{
			try 
			{
				if(m_socket.isConnected())
					return "Connected";
				else
					return "Disconnected";
			}
			catch(Exception e)
			{
				return "Unknown";
			}
		}

		
		public bool isConnected() 
		{
			if(m_socket.isConnected())
				return true;
			return false;		
		}

		public Status getStatus() 
		{
			try 
			{
				if(m_socket.isConnected())
					return Status.Connected;
				else
					return Status.Disconnected;
			}
			catch(Exception e)
			{
				return Status.Unknown;
			}
		}

		public void setStatus(Status status) 
		{
			m_status=status;
		}

		public void addProcess(Process process) 
		{
			m_processes.Add(process);
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


		public bool removeProcess(string name, string database)
		{
			foreach(Process p in m_processes)
			{
				if(p.getName().Equals(name) && p.getDatabase().Equals(database))
				{
					m_processes.Remove(p);
					return true;
				}
			}
			return false;
		}

		public void disconnect()
		{	
			m_socket.disconnect();
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

		public int listProcesses()
		{
			string list = "list processes\n\n";

			if(!sendMessage(list))
				return -2;
			
			SimpleCPCParser.parse(m_processes, this, m_socket);
			return 1;
		}

		public int defineProcess(Process p)
		{
			string define = "define process \n";
			define = define + "name:" + p.getName() + "\n";
			define = define + "group:" + p.getDatabase() + "\n";
			define = define + "env:" + "NDB_CONNECTSTRING="+p.getConnectString() ;
			if(p.getEnv().Equals(""))
				define = define + "\n";
			else
				define = define + " " + p.getEnv() + "\n";
				
			//if(p.getPath().EndsWith("\\")) 
			//	define = define + "path:" + p.getPath()+ "ndb" + "\n";
			//else
			define = define + "path:" + p.getPath() + "\n";
			define = define + "args:" + p.getArgs() + "\n";
			define = define + "type:" + "permanent" + "\n";
			define = define + "cwd:" + p.getCwd() + "\n";
			define = define + "owner:" + "ejohson" + "\n\n";
			
			if(!sendMessage(define))
				return -2;
			
			SimpleCPCParser.parse(p, m_socket);
			if(p.isDefined())
				return 1;
			else 
				return -1;
		
		}

		public int startProcess(Process p)
		{
			if(!p.isDefined())
			{
				this.defineProcess(p);
				if(!p.isDefined())
					return -4; //process misconfigured

			}
			string start= "start process \n";
			start = start + "id:" + p.getId() + "\n\n";
			if(!sendMessage(start))
				return -2;
			SimpleCPCParser.parse(p, m_socket);
			if(p.getStatus().Equals(Process.Status.Running))
				return 1;
			else 
				return -1;
		}

		public int stopProcess(Process p)
		{
			if(!p.isDefined())
			{
				return -4; //process not defined
			}
			string stop= "stop process \n";
			stop = stop + "id:" + p.getId() + "\n\n";
			if(!sendMessage(stop))
				return -2;
			SimpleCPCParser.parse(p, m_socket);
	
			if(p.getStatus().Equals(Process.Status.Stopped))
				return 1;
			else 
				return -1;
		}

		public int undefineProcess(Process p)
		{
			if(!p.isDefined())
			{
				return -4; //process not defined
			}
			string undefine= "undefine process \n";
			undefine = undefine + "id:" + p.getId() + "\n\n";
			if(!sendMessage(undefine))
				return -2;
			SimpleCPCParser.parse(p, m_socket);
			if(!p.isDefined()) 
			{
				return 1;

			}
			return -1;
		}

		public int getCpcdPort()
		{
			return this.m_cpcdPort;
		}

	}
}
