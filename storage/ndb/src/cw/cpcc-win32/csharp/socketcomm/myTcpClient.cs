using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.IO;


namespace NDB_CPC.socketcomm
{
	public class myTcpClient : TcpClient
		{
			private Socket s;
			public myTcpClient(): base()
			{
				if(this.Active) 
				{
					s = this.Client;
				}
			}
			public Socket GetUnderlyingSocket()
			{
				return s;
			}
		}
}
