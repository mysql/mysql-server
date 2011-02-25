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
using System.Text;
using System.Collections.Specialized;
using System.IO;
using System.Windows.Forms;
namespace NDB_CPC.fileaccess
{
	/// <summary>
	/// Summary description for FileMgmt.
	/// </summary>
	public  class FileMgmt
	{
		public  FileMgmt()
		{
		}
		
		public StringCollection importHostFile(string filename)
		{
			StringCollection sc = new StringCollection();
			StreamReader SR = new StreamReader(filename);
			string line ="";
			line = SR.ReadLine();
			while(!line.Equals(""))
			{
				sc.Add(line);
				line = SR.ReadLine();
			}
			return sc;
		}

		public void exportHostFile(string filename, string content)
		{
			StreamWriter SW = new StreamWriter(filename,false);
			SW.Write(content);
			SW.WriteLine("");
			SW.WriteLine("");
			SW.Close();
		}

	}
}
