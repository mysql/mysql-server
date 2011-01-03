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
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Windows.Forms;
using System.Threading;
using System.IO;

namespace NDB_CPC.socketcomm
{
	/// <summary>
	/// Summary description for SocketComm.
	/// </summary>
	public class SocketComm
	{
		private myTcpClient sender;
		private StreamWriter writer;
		private StreamReader reader;
		private string m_host;
		private int m_port;
		private bool m_connected;
		private bool m_connecting;
		private Thread connectThread;
		public SocketComm(string host, int port)
		{

			m_host=host;
			m_port=port;
			m_connected=false;
			m_connecting=false;
		}

		

		public bool isConnected()
		{
			return m_connected;
		}

		public void doConnect()
		{
			if(!m_connecting && !m_connected) 
			{
				connectThread= new Thread(new ThreadStart(connect));
				connectThread.Start();
			}
					
		}

		private void connect()
		{
			m_connecting=true;
			while(true) 
			{
				if(!m_connected)
				{
					try 
					{
						// Establish the remote endpoint for the socket.
						//    The name of the
						//   remote device is "host.contoso.com".
						
						// Create a TCP/IP  socket.
						sender = new myTcpClient();
						// Connect the socket to the remote endpoint. Catch any errors.
						try 
						{
							/*
							IPAddress ipAddress = Dns.Resolve(host).AddressList[0];
							IPEndPoint ipLocalEndPoint = new IPEndPoint(ipAddress, 11000);
*/

							
							sender.Connect(m_host,m_port);;

							writer = new StreamWriter(sender.GetStream(), Encoding.ASCII);
							reader = new StreamReader(sender.GetStream(), Encoding.ASCII);
							m_connected=true;
							m_connecting=false;
				//			break;
							Console.WriteLine("Socket connected to {0}",
								sender.ToString());
       
						} 
						catch (ArgumentNullException ane) 
						{
							Console.WriteLine("ArgumentNullException : {0}",ane.ToString());
							m_connected=false;
						} 
						catch (SocketException se) 
						{
							Console.WriteLine("SocketException : {0}",se.ToString());
							m_connected=false;
						} 
					}
					catch (Exception e) 
					{
						Console.WriteLine("Unexpected exception : {0}", e.ToString());
						m_connected=false;
					}
				
				}
				
				Thread.Sleep(200);
			}
		}

		public bool disconnect()
		{
			try 
			{
				this.m_connected=false;
				this.m_connecting=false;
				sender.GetUnderlyingSocket().Shutdown(SocketShutdown.Both);
				sender.GetUnderlyingSocket().Close();
				writer.Close();
				reader.Close();
				sender.Close();
        
			} 
			catch (ArgumentNullException ane) 
			{
				Console.WriteLine("ArgumentNullException : {0}",ane.ToString());
				connectThread.Abort();
				return false;
			} 
			catch (SocketException se) 
			{
				Console.WriteLine("SocketException : {0}",se.ToString());
				connectThread.Abort();
				return false;
			} 
			catch (Exception e) 
			{
				Console.WriteLine("Unexpected exception : {0}", e.ToString());
				connectThread.Abort();
				return false;
			}
			connectThread.Abort();
			return true;
		}
		
		public bool writeMessage(string message) 
		{
			int attempts=0;
			while (attempts < 10)
			{
				try 
				{
					writer.WriteLine(message);
					writer.Flush();
					message="";
					return true;
				} 
				catch(IOException e)
				{
					this.disconnect();
					this.doConnect();
					Thread.Sleep(200);
					attempts++;
				}
				catch(System.NullReferenceException)
				{
					this.disconnect();
					this.doConnect();
			
					Thread.Sleep(200);
					attempts++;
				}
			}
			return false;
		}

		public string readLine() 
		{
			int attempts=0;
			string line="";
			while (attempts < 10){
				try 
				{
					line = reader.ReadLine();
					if(line==null)
						line="";
					return line;
				}
				catch(IOException e)
				{
					this.disconnect();
					this.doConnect();
					Thread.Sleep(400);
					attempts++;
				}
				catch(System.NullReferenceException)
				{
					this.disconnect();
					this.doConnect();
					Thread.Sleep(400);
					attempts++;
				}
			}
			return "";
			
		}
		
	}
}

