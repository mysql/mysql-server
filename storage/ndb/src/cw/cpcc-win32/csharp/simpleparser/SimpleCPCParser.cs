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
using System.Collections;
using System.IO;
using System.Windows.Forms;
using NDB_CPC;
using NDB_CPC.socketcomm;

namespace NDB_CPC.simpleparser
{
	/// <summary>
	/// Summary description for SimpleCPCParser.
	/// </summary>
	public class SimpleCPCParser
	{
		public SimpleCPCParser()
		{
			//
			// TODO: Add constructor logic here
			//
		}

		public static void parse(Process p, SocketComm comm)
		{
			
			string line=comm.readLine();//reader.ReadLine();
			while(line.Equals("")) 
			{
				line=comm.readLine();
			}
			if(line.Equals("define process"))
			{
				defineProcess(p, comm);
				line="";
				return;
			}
			if(line.Equals("start process"))
			{
				startProcess(p,comm);
				line="";
				return;
			}
			if(line.Equals("stop process"))
			{
				stopProcess(p,comm);
				line="";
				return;
			}
			if(line.Equals("undefine process"))
			{
				undefineProcess(p,comm);
				line="";
				return;
			}

		}

		public static void parse(ArrayList processes, Computer c, SocketComm comm)
		{
			
			string line=comm.readLine();//reader.ReadLine();
			while(line.Equals("")) 
			{
				line=comm.readLine();
			}

			if(line.Equals("start processes"))
			{
				listProcesses(processes, c, comm);
				line="";
				return;
			}
			
		}
		
		private static void defineProcess(Process p, SocketComm comm)
		{
			string line=comm.readLine();//reader.ReadLine();
			while(!line.Equals(""))
			{
				if(line.StartsWith("status:"))
				{
					line=line.Remove(0,7);
					line=line.Trim();
					if(line.Equals("1")) 
					{
						p.setDefined(true);
						p.setStatus(Process.Status.Stopped);
					}
					else 
						p.setDefined(false);
				}
				if(line.StartsWith("id:"))
				{
					line=line.Remove(0,3);
					line=line.Trim();
					p.setId(line);
				}
				line=comm.readLine();
			}
		}

		
		private static void startProcess(Process p, SocketComm comm)
		{
			string line=comm.readLine();//reader.ReadLine();
			while(!line.Equals(""))
			{
				if(line.StartsWith("status:"))
				{					
					line=line.Remove(0,7);
					line=line.Trim();
					if(line.Equals("1"))
						p.setStatus(NDB_CPC.Process.Status.Running);
					else
						p.setStatus(NDB_CPC.Process.Status.Unknown);
	
				}
				if(line.StartsWith("id:"))
				{
					line=line.Remove(0,3);
					line=line.Trim();
					if(p.getId().Equals(line))
					{
						;
					}
					else
					{
						//damn something is wrong
						p.setStatus(NDB_CPC.Process.Status.Unknown);
					}
					
				}
				line=comm.readLine();
			}
		}
		private static void undefineProcess(Process p, SocketComm comm)
		{
			string line=comm.readLine();//reader.ReadLine();
			while(!line.Equals(""))
			{
				if(line.StartsWith("status:"))
				{
					
					line=line.Remove(0,7);
					line=line.Trim();
					if(line.Equals("1"))
						p.setDefined(false);
					else
						p.setDefined(true);
	
				}
				if(line.StartsWith("id:"))
				{
					line=line.Remove(0,3);
					line=line.Trim();
				}
				line=comm.readLine();
			}
		}

		private static void stopProcess(Process p, SocketComm comm)
		{
			string line=comm.readLine();//reader.ReadLine();
			while(!line.Equals(""))
			{
				if(line.StartsWith("status:"))
				{					
					line=line.Remove(0,7);
					line=line.Trim();
					if(line.Equals("1"))
						p.setStatus(NDB_CPC.Process.Status.Stopped);
					else
						p.setStatus(NDB_CPC.Process.Status.Unknown);
	
				}
				if(line.StartsWith("id:"))
				{
					line=line.Remove(0,3);
					line=line.Trim();
					if(p.getId().Equals(line))
					{
						;
					}
					else
					{
						//damn something is wrong
						p.setStatus(NDB_CPC.Process.Status.Unknown);
					}
					
				}
				line=comm.readLine();
			}
		}
		private static void listProcesses(ArrayList processes, Computer c, SocketComm comm)
		{
			bool processExist = false;
			
			string line=comm.readLine();//reader.ReadLine();
			while(!line.Equals("end processes"))
			{
				if(line.Equals("process")) 
				{
					line=comm.readLine();
					Process p = new Process();
						
					while(!line.Equals("")) 
					{
						if(line.StartsWith("id:"))
						{
							string pid;
							line=line.Remove(0,3);
							pid=line.Trim();
							/*check if process already exist*/
							processExist=findProcess(processes,pid);
							if(!processExist) 
							{
								p.setId(pid);
							}
						}

						if(line.StartsWith("name:"))
						{
							
							line=line.Remove(0,5);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setName(line);
							}
						}
					
						if(line.StartsWith("path:"))
						{
							
							line=line.Remove(0,5);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setPath(line);
							}
						}

						if(line.StartsWith("args:"))
						{
							
							line=line.Remove(0,5);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setArgs(line);
							}
						}

						if(line.StartsWith("type:"))
						{
							
							line=line.Remove(0,5);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								
							}
						}
						
						if(line.StartsWith("cwd:"))
						{
							
							line=line.Remove(0,4);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setCwd(line);
							}
						}

						if(line.StartsWith("env:"))
						{
							
							line=line.Remove(0,4);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setEnv(line);
							}
						}
						
						if(line.StartsWith("owner:"))
						{
							
							line=line.Remove(0,6);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setOwner(line);
							}
						}
						if(line.StartsWith("group:"))
						{
							
							line=line.Remove(0,6);
							line=line.Trim();
							/*check if process already exist*/
							if(!processExist) 
							{
								p.setDatabase(line);
							}
						}

						if(line.StartsWith("status:"))
						{
							
							line=line.Remove(0,7);
							line=line.Trim();
							/*check if process already exist*/
							//if(!processExist) 
							//{
								if(line.Equals("0"))
									p.setStatus(Process.Status.Stopped);
								if(line.Equals("1"))
									p.setStatus(Process.Status.Running);
								if(line.Equals("2"))
									p.setStatus(Process.Status.Unknown);
							//}
						}


						line=comm.readLine();
					}
					if(!processExist) 
					{
						p.setComputer(c);
						p.setDefined(true);
						processes.Add(p);
					}
					processExist=false;
				}
				line=comm.readLine();	
				
			}
	}

		private static bool findProcess(ArrayList processes, string pid)
		{
			foreach (Process p in processes) 
			{
				if(p.getId().Equals(pid))
					return true;
			}
			return false;

		}
	}
}
