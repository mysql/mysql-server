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
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.IO;
using System.Threading ;

namespace NDB_CPC.telnetclient
{
	/// <summary>
	/// Summary description for telnetClient.
	/// </summary>
	public class telnetClient
	{
		Char IAC				= Convert.ToChar(255);
		Char DO					= Convert.ToChar(253);
		Char DONT				= Convert.ToChar(254);
		Char WILL				= Convert.ToChar(251);
		Char WONT				= Convert.ToChar(252);
		Char SB					= Convert.ToChar(250);
		Char SE					= Convert.ToChar(240);
		const	Char IS			= '0';
		const	Char SEND		= '1';
		const	Char INFO		= '2';
		const	Char VAR		= '0';
		const	Char VALUE		= '1';
		const	Char ESC		= '2';
		const	Char USERVAR	= '3';
		string m_strResp;

		private ArrayList  m_ListOptions = new ArrayList();
		private IPEndPoint iep ;
		private AsyncCallback callbackProc ;
		private string address ;
		private int port ;
		private Socket s ;
		private TextBox textBox1;
		Byte[] m_byBuff = new Byte[32767];


		public telnetClient(string ip, int p, TextBox tb)
		{

			address = ip;
			port = p;
			textBox1=tb;
			IPHostEntry IPHost = Dns.Resolve(address); 
			string []aliases = IPHost.Aliases; 
			IPAddress[] addr = IPHost.AddressList; 
		
			try
			{
				// Create New Socket 
				s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
				// Create New EndPoint
				iep	= new IPEndPoint(addr[0],port);  
				// This is a non blocking IO
				s.Blocking		= false ;	
				// Assign Callback function to read from Asyncronous Socket
				callbackProc	= new AsyncCallback(ConnectCallback);
				// Begin Asyncronous Connection
				s.BeginConnect(iep , callbackProc, s ) ;				
		
			}
			catch(Exception eeeee )
			{
				MessageBox.Show(eeeee.Message , "Application Error!!!" , MessageBoxButtons.OK , MessageBoxIcon.Stop );
				Application.Exit();
			}
		}

		public void ConnectCallback( IAsyncResult ar )
		{
			try
			{
				// Get The connection socket from the callback
				Socket sock1 = (Socket)ar.AsyncState;
				if ( sock1.Connected ) 
				{	
					// Define a new Callback to read the data 
					AsyncCallback recieveData = new AsyncCallback( OnRecievedData );
					// Begin reading data asyncronously
					sock1.BeginReceive( m_byBuff, 0, m_byBuff.Length, SocketFlags.None, recieveData , sock1 );
				}
			}
			catch( Exception ex )
			{
				MessageBox.Show(ex.Message, "Setup Recieve callbackProc failed!" );
			}
		}

		
		public void OnRecievedData( IAsyncResult ar )
		{
			// Get The connection socket from the callback
			Socket sock = (Socket)ar.AsyncState;
				// Get The data , if any
				int nBytesRec = sock.EndReceive( ar );		
				if( nBytesRec > 0 )
				{
					string sRecieved = Encoding.ASCII.GetString( m_byBuff, 0, nBytesRec );
					string m_strLine="";
					for ( int i=0; i < nBytesRec;i++)
					{
						Char ch = Convert.ToChar(m_byBuff[i]);
						switch( ch )
						{
							case '\r': 
								m_strLine += Convert.ToString("\r\n"); 
								break;
							case '\n': 
								break;
							default: 
								m_strLine += Convert.ToString(ch);
								break;
						} 
					}
					try
					{
						int strLinelen = m_strLine.Length ;
						if ( strLinelen == 0 ) 
						{
							m_strLine = Convert.ToString("\r\n");
						}

						Byte[] mToProcess = new Byte[strLinelen];
						for ( int i=0; i <  strLinelen ; i++)
							mToProcess[i] = Convert.ToByte(m_strLine[i]);
						// Process the incoming data
						string mOutText = ProcessOptions(mToProcess);
						if ( mOutText != "" ) 
							textBox1.AppendText(mOutText);
				
						// Respond to any incoming commands
						RespondToOptions();
					}
					catch( Exception ex )
					{
						Object x = this ;
						MessageBox.Show(ex.Message , "Information!" );
					}
				}
				else
				{
					// If no data was recieved then the connection is probably dead
					Console.WriteLine( "Disconnected", sock.RemoteEndPoint );
					sock.Shutdown( SocketShutdown.Both );
					sock.Close();
					Application.Exit();
				}
		}
	
		private string ProcessOptions(byte[] m_strLineToProcess)
		{
			string m_DISPLAYTEXT	="";
			string m_strTemp		="" ;
			string m_strOption		="";
			string m_strNormalText	="";
			bool bScanDone			=false;
			int ndx					=0;
			int ldx					=0;
			char ch	;
			try
			{
				for ( int i=0; i < m_strLineToProcess.Length ; i++)
				{
					Char ss = Convert.ToChar(m_strLineToProcess[i]);
					m_strTemp = m_strTemp + Convert.ToString(ss);
				}

				while(bScanDone != true )
				{
					int lensmk = m_strTemp.Length;
					ndx = m_strTemp.IndexOf(Convert.ToString(IAC));
					if ( ndx > lensmk )
						ndx = m_strTemp.Length;

					if(ndx != -1)
					{
						m_DISPLAYTEXT+= m_strTemp.Substring(0,ndx);
						ch = m_strTemp[ndx + 1];
						if ( ch == DO || ch == DONT || ch == WILL || ch == WONT ) 
						{
							m_strOption		= m_strTemp.Substring(ndx, 3);
							string txt		= m_strTemp.Substring(ndx + 3);
							m_DISPLAYTEXT+= m_strTemp.Substring(0,ndx);
							m_ListOptions.Add(m_strOption);
							m_strTemp		= txt ;
						}
						else
							if ( ch == IAC)
						{
							m_DISPLAYTEXT= m_strTemp.Substring(0,ndx);
							m_strTemp		= m_strTemp.Substring(ndx + 1);
						}
						else
							if ( ch == SB ) 
						{
							m_DISPLAYTEXT= m_strTemp.Substring(0,ndx);
							ldx = m_strTemp.IndexOf(Convert.ToString(SE));
							m_strOption		= m_strTemp.Substring(ndx, ldx);
							m_ListOptions.Add(m_strOption);
							m_strTemp			= m_strTemp.Substring(ldx);
						}
					}
					else
					{
						m_DISPLAYTEXT = m_DISPLAYTEXT + m_strTemp;
						bScanDone = true ;
					}
				} 
				m_strNormalText = m_DISPLAYTEXT; 	
			}
			catch(Exception eP)
			{
				MessageBox.Show(eP.Message , "Application Error!!!" , MessageBoxButtons.OK , MessageBoxIcon.Stop );
				Application.Exit();
			}
			return m_strNormalText ;
		}
	
		void DispatchMessage(string strText)
		{
			try
			{
				Byte[] smk = new Byte[strText.Length];
				for ( int i=0; i < strText.Length ; i++)
				{
					Byte ss = Convert.ToByte(strText[i]);
					smk[i] = ss ;
				}

				IAsyncResult ar2 = s.BeginSend(smk , 0 , smk.Length , SocketFlags.None , callbackProc , s );
				s.EndSend(ar2);
			}
			catch(Exception ers)
			{
				MessageBox.Show("ERROR IN RESPOND OPTIONS");
			}
		}

		void RespondToOptions()
		{
			try
			{
				string strOption;
				for ( int i=0; i < m_ListOptions.Count; i++)
				{
					strOption = (string)m_ListOptions[i];
					ArrangeReply(strOption);
				}
				DispatchMessage(m_strResp);
				m_strResp ="";
				m_ListOptions.Clear();
			}
			catch(Exception ers)
			{
				MessageBox.Show("ERROR IN RESPOND OPTIONS");
			}
		}
		void ArrangeReply(string strOption)
		{
			try
			{

				Char Verb;
				Char Option;
				Char Modifier;
				Char ch;
				bool bDefined = false;

				if(strOption.Length  < 3) return;

				Verb = strOption[1];
				Option = strOption[2];

				if ( Option == 1 || Option == 3 ) 
				{
					//				case 1:	// Echo
					//				case 3: // Suppress Go-Ahead
					bDefined = true;
					//					break;
				}

				m_strResp += IAC;

				if(bDefined == true )
				{
					if ( Verb == DO ) 
					{
						//					case DO:
						ch = WILL;
						m_strResp += ch;
						m_strResp += Option;
						//						break;
					}
					if ( Verb == DONT ) 
					{
						ch = WONT;
						m_strResp += ch;
						m_strResp += Option;
						//		break;
					}
					if ( Verb == WILL ) 
					{
						ch = DO;
						m_strResp += ch;
						m_strResp += Option;
						//break;
					}
					if ( Verb == WONT)
					{
						ch = DONT;
						m_strResp += ch;
						m_strResp += Option;
						//	break;
					}
					if ( Verb == SB)
					{
						Modifier = strOption[3];
						if(Modifier == SEND)
						{
							ch = SB;
							m_strResp += ch;
							m_strResp += Option;
							m_strResp += IS;
							m_strResp += IAC;
							m_strResp += SE;
						}
						//						break;
					}
				}
				else
				{
					//				switch(Verb)
					//				{
					if ( Verb == DO ) 
					{
						ch = WONT;
						m_strResp += ch;
						m_strResp += Option;
						//	break;
					}
					if ( Verb == DONT)
					{
						ch = WONT;
						m_strResp += ch;
						m_strResp += Option;
						//	break;
					}
					if ( Verb == WILL)
					{
						ch = DONT;
						m_strResp += ch;
						m_strResp += Option;
						//	break;
					}
					if ( Verb == WONT)
					{
						ch = DONT;
						m_strResp += ch;
						m_strResp += Option;
						//		break;
					}
				}
			}
			catch(Exception eeeee )
			{
				MessageBox.Show(eeeee.Message , "Application Error!!!" , MessageBoxButtons.OK , MessageBoxIcon.Stop );
				Application.Exit();
			}

		}

		private void textBox1_KeyPress_1(object sender, System.Windows.Forms.KeyPressEventArgs e)
		{
			if ( e.KeyChar == 13 ) 
			{
				DispatchMessage("\r\n");
			}
			else
			if ( e.KeyChar == 8 )
			{
				try
				{
//					string mtmp = textBox1.Text.Substring(0,textBox1.Text.Length-1);
//					textBox1.Text = "" ;		 
				}
				catch(Exception ebs)
				{
					MessageBox.Show("ERROR IN BACKSPACE");
				}
			}
			else
			{
				string str = e.KeyChar.ToString();
				DispatchMessage(str);
			}
		}


	}
}
