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
