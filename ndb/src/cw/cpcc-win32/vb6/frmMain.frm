VERSION 5.00
Object = "{831FDD16-0C5C-11D2-A9FC-0000F8754DA1}#2.0#0"; "mscomctl.ocx"
Begin VB.Form frmMain 
   Caption         =   "NdbCPC"
   ClientHeight    =   5955
   ClientLeft      =   2115
   ClientTop       =   2250
   ClientWidth     =   8880
   LinkTopic       =   "Form1"
   ScaleHeight     =   5955
   ScaleWidth      =   8880
   Begin MSComctlLib.ImageList ImageList1 
      Left            =   6840
      Top             =   3120
      _ExtentX        =   1005
      _ExtentY        =   1005
      BackColor       =   16777215
      ImageWidth      =   16
      ImageHeight     =   16
      MaskColor       =   12632256
      _Version        =   393216
      BeginProperty Images {2C247F25-8591-11D1-B16A-00C0F0283628} 
         NumListImages   =   11
         BeginProperty ListImage1 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":0000
            Key             =   "close"
         EndProperty
         BeginProperty ListImage2 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":27B4
            Key             =   "open"
         EndProperty
         BeginProperty ListImage3 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":4F68
            Key             =   "computer_unknown"
         EndProperty
         BeginProperty ListImage4 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":5284
            Key             =   "computer_stopped"
         EndProperty
         BeginProperty ListImage5 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":55A0
            Key             =   "computer_started"
         EndProperty
         BeginProperty ListImage6 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":58BC
            Key             =   ""
         EndProperty
         BeginProperty ListImage7 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":5BD8
            Key             =   ""
         EndProperty
         BeginProperty ListImage8 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":5EF4
            Key             =   ""
         EndProperty
         BeginProperty ListImage9 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":6210
            Key             =   "db"
         EndProperty
         BeginProperty ListImage10 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":652A
            Key             =   "computer"
         EndProperty
         BeginProperty ListImage11 {2C247F27-8591-11D1-B16A-00C0F0283628} 
            Picture         =   "frmMain.frx":6844
            Key             =   "properties"
         EndProperty
      EndProperty
   End
   Begin VB.PictureBox picSplitter 
      BackColor       =   &H00808080&
      BorderStyle     =   0  'None
      FillColor       =   &H00808080&
      Height          =   4800
      Left            =   5400
      ScaleHeight     =   2090.126
      ScaleMode       =   0  'User
      ScaleWidth      =   780
      TabIndex        =   6
      Top             =   705
      Width           =   72
      Visible         =   0   'False
   End
   Begin MSComctlLib.TreeView tvTreeView 
      Height          =   4800
      Left            =   0
      TabIndex        =   5
      Top             =   705
      Width           =   2016
      _ExtentX        =   3545
      _ExtentY        =   8467
      _Version        =   393217
      HideSelection   =   0   'False
      Indentation     =   0
      LineStyle       =   1
      Sorted          =   -1  'True
      Style           =   7
      FullRowSelect   =   -1  'True
      ImageList       =   "ImageList1"
      Appearance      =   1
   End
   Begin VB.PictureBox picTitles 
      Align           =   1  'Align Top
      Appearance      =   0  'Flat
      BorderStyle     =   0  'None
      ForeColor       =   &H80000008&
      Height          =   300
      Left            =   0
      ScaleHeight     =   300
      ScaleWidth      =   8880
      TabIndex        =   2
      TabStop         =   0   'False
      Top             =   420
      Width           =   8880
      Begin VB.Label lblTitle 
         BorderStyle     =   1  'Fixed Single
         Caption         =   " ListView:"
         Height          =   270
         Index           =   1
         Left            =   2078
         TabIndex        =   4
         Tag             =   " ListView:"
         Top             =   12
         Width           =   3216
      End
      Begin VB.Label lblTitle 
         BorderStyle     =   1  'Fixed Single
         Caption         =   " TreeView:"
         Height          =   270
         Index           =   0
         Left            =   0
         TabIndex        =   3
         Tag             =   " TreeView:"
         Top             =   12
         Width           =   2016
      End
   End
   Begin MSComctlLib.Toolbar tbToolBar 
      Align           =   1  'Align Top
      Height          =   420
      Left            =   0
      TabIndex        =   1
      Top             =   0
      Width           =   8880
      _ExtentX        =   15663
      _ExtentY        =   741
      ButtonWidth     =   609
      ButtonHeight    =   582
      Appearance      =   1
      ImageList       =   "ImageList1"
      _Version        =   393216
      BeginProperty Buttons {66833FE8-8583-11D1-B16A-00C0F0283628} 
         NumButtons      =   5
         BeginProperty Button1 {66833FEA-8583-11D1-B16A-00C0F0283628} 
            Style           =   3
         EndProperty
         BeginProperty Button2 {66833FEA-8583-11D1-B16A-00C0F0283628} 
            Key             =   "Add computer"
            Object.ToolTipText     =   "Add computer"
            ImageKey        =   "computer"
         EndProperty
         BeginProperty Button3 {66833FEA-8583-11D1-B16A-00C0F0283628} 
            Key             =   "New database"
            Object.ToolTipText     =   "New database"
            ImageKey        =   "db"
         EndProperty
         BeginProperty Button4 {66833FEA-8583-11D1-B16A-00C0F0283628} 
            Style           =   3
         EndProperty
         BeginProperty Button5 {66833FEA-8583-11D1-B16A-00C0F0283628} 
            Key             =   "Properties"
            Object.ToolTipText     =   "Properties"
            ImageKey        =   "properties"
         EndProperty
      EndProperty
   End
   Begin MSComctlLib.StatusBar sbStatusBar 
      Align           =   2  'Align Bottom
      Height          =   270
      Left            =   0
      TabIndex        =   0
      Top             =   5685
      Width           =   8880
      _ExtentX        =   15663
      _ExtentY        =   476
      _Version        =   393216
      BeginProperty Panels {8E3867A5-8586-11D1-B16A-00C0F0283628} 
         NumPanels       =   3
         BeginProperty Panel1 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            AutoSize        =   1
            Object.Width           =   10028
            Text            =   "Status"
            TextSave        =   "Status"
         EndProperty
         BeginProperty Panel2 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            Style           =   6
            AutoSize        =   2
            TextSave        =   "2002-10-15"
         EndProperty
         BeginProperty Panel3 {8E3867AB-8586-11D1-B16A-00C0F0283628} 
            Style           =   5
            AutoSize        =   2
            TextSave        =   "09:44"
         EndProperty
      EndProperty
   End
   Begin MSComctlLib.ListView lvProcesses 
      Height          =   4815
      Left            =   2040
      TabIndex        =   8
      Top             =   720
      Width           =   3255
      _ExtentX        =   5741
      _ExtentY        =   8493
      Sorted          =   -1  'True
      MultiSelect     =   -1  'True
      LabelWrap       =   -1  'True
      HideSelection   =   0   'False
      AllowReorder    =   -1  'True
      FullRowSelect   =   -1  'True
      _Version        =   393217
      ForeColor       =   -2147483640
      BackColor       =   -2147483643
      BorderStyle     =   1
      Appearance      =   1
      NumItems        =   6
      BeginProperty ColumnHeader(1) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         Key             =   "Id"
         Text            =   "Id"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(2) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   1
         Key             =   "Computer"
         Text            =   "Computer"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(3) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   2
         Key             =   "Database"
         Text            =   "Database"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(4) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   3
         Key             =   "Name"
         Text            =   "Name"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(5) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   4
         Key             =   "Status"
         Text            =   "Status"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(6) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   5
         Key             =   "Owner"
         Text            =   "Owner"
         Object.Width           =   2540
      EndProperty
   End
   Begin MSComctlLib.ListView lvComputers 
      Height          =   4815
      Left            =   2040
      TabIndex        =   7
      Top             =   720
      Width           =   3255
      _ExtentX        =   5741
      _ExtentY        =   8493
      Sorted          =   -1  'True
      MultiSelect     =   -1  'True
      LabelWrap       =   -1  'True
      HideSelection   =   -1  'True
      AllowReorder    =   -1  'True
      FullRowSelect   =   -1  'True
      _Version        =   393217
      Icons           =   "ImageList1"
      SmallIcons      =   "ImageList1"
      ForeColor       =   -2147483640
      BackColor       =   -2147483643
      BorderStyle     =   1
      Appearance      =   1
      NumItems        =   2
      BeginProperty ColumnHeader(1) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         Text            =   "Computer"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(2) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   1
         Text            =   "Status"
         Object.Width           =   2540
      EndProperty
   End
   Begin MSComctlLib.ListView lvDatabases 
      Height          =   4815
      Left            =   2040
      TabIndex        =   9
      Top             =   720
      Width           =   3255
      _ExtentX        =   5741
      _ExtentY        =   8493
      View            =   3
      Sorted          =   -1  'True
      MultiSelect     =   -1  'True
      LabelWrap       =   -1  'True
      HideSelection   =   -1  'True
      AllowReorder    =   -1  'True
      FullRowSelect   =   -1  'True
      _Version        =   393217
      Icons           =   "ImageList1"
      SmallIcons      =   "ImageList1"
      ForeColor       =   -2147483640
      BackColor       =   -2147483643
      BorderStyle     =   1
      Appearance      =   1
      NumItems        =   2
      BeginProperty ColumnHeader(1) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         Key             =   "Database"
         Text            =   "Database"
         Object.Width           =   2540
      EndProperty
      BeginProperty ColumnHeader(2) {BDD1F052-858B-11D1-B16A-00C0F0283628} 
         SubItemIndex    =   1
         Key             =   "Status"
         Text            =   "Status"
         Object.Width           =   2540
      EndProperty
   End
   Begin VB.Image imgSplitter 
      Height          =   4788
      Left            =   1965
      MousePointer    =   9  'Size W E
      Top             =   705
      Width           =   150
   End
   Begin VB.Menu mnuFile 
      Caption         =   "&File"
      Begin VB.Menu mnuFileOpen 
         Caption         =   "&Open..."
      End
      Begin VB.Menu mnuFileFind 
         Caption         =   "&Find"
      End
      Begin VB.Menu mnuFileBar0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileSendTo 
         Caption         =   "Sen&d to"
      End
      Begin VB.Menu mnuFileBar1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileNew 
         Caption         =   "&New"
         Shortcut        =   ^N
      End
      Begin VB.Menu mnuFileBar2 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileDelete 
         Caption         =   "&Delete"
      End
      Begin VB.Menu mnuFileRename 
         Caption         =   "Rena&me"
      End
      Begin VB.Menu mnuFileProperties 
         Caption         =   "Propert&ies"
      End
      Begin VB.Menu mnuFileBar3 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileMRU 
         Caption         =   ""
         Index           =   1
         Visible         =   0   'False
      End
      Begin VB.Menu mnuFileMRU 
         Caption         =   ""
         Index           =   2
         Visible         =   0   'False
      End
      Begin VB.Menu mnuFileMRU 
         Caption         =   ""
         Index           =   3
         Visible         =   0   'False
      End
      Begin VB.Menu mnuFileBar4 
         Caption         =   "-"
         Visible         =   0   'False
      End
      Begin VB.Menu mnuFileBar5 
         Caption         =   "-"
      End
      Begin VB.Menu mnuFileClose 
         Caption         =   "&Close"
      End
   End
   Begin VB.Menu mnuEdit 
      Caption         =   "&Edit"
      Begin VB.Menu mnuEditUndo 
         Caption         =   "&Undo"
      End
      Begin VB.Menu mnuEditBar0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuEditCut 
         Caption         =   "Cu&t"
         Shortcut        =   ^X
      End
      Begin VB.Menu mnuEditCopy 
         Caption         =   "&Copy"
         Shortcut        =   ^C
      End
      Begin VB.Menu mnuEditPaste 
         Caption         =   "&Paste"
         Shortcut        =   ^V
      End
      Begin VB.Menu mnuEditPasteSpecial 
         Caption         =   "Paste &Special..."
      End
      Begin VB.Menu mnuEditBar1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuEditSelectAll 
         Caption         =   "Select &All"
         Shortcut        =   ^A
      End
      Begin VB.Menu mnuEditInvertSelection 
         Caption         =   "&Invert Selection"
      End
   End
   Begin VB.Menu mnuView 
      Caption         =   "&View"
      Begin VB.Menu mnuViewToolbar 
         Caption         =   "&Toolbar"
         Checked         =   -1  'True
      End
      Begin VB.Menu mnuViewStatusBar 
         Caption         =   "Status &Bar"
         Checked         =   -1  'True
      End
      Begin VB.Menu mnuViewBar0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuListViewMode 
         Caption         =   "Lar&ge Icons"
         Index           =   0
      End
      Begin VB.Menu mnuListViewMode 
         Caption         =   "S&mall Icons"
         Index           =   1
      End
      Begin VB.Menu mnuListViewMode 
         Caption         =   "&List"
         Index           =   2
      End
      Begin VB.Menu mnuListViewMode 
         Caption         =   "&Details"
         Index           =   3
      End
      Begin VB.Menu mnuViewBar1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuViewArrangeIcons 
         Caption         =   "Arrange &Icons"
      End
      Begin VB.Menu mnuViewBar2 
         Caption         =   "-"
      End
      Begin VB.Menu mnuViewRefresh 
         Caption         =   "&Refresh"
      End
      Begin VB.Menu mnuViewOptions 
         Caption         =   "&Options..."
      End
      Begin VB.Menu mnuViewWebBrowser 
         Caption         =   "&Web Browser"
      End
   End
   Begin VB.Menu mnuHelp 
      Caption         =   "&Help"
      Begin VB.Menu mnuHelpContents 
         Caption         =   "&Contents"
      End
      Begin VB.Menu mnuHelpSearchForHelpOn 
         Caption         =   "&Search For Help On..."
      End
      Begin VB.Menu mnuHelpBar0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuHelpAbout 
         Caption         =   "&About "
      End
   End
   Begin VB.Menu mnuPopComputers 
      Caption         =   ""
      Visible         =   0   'False
      Begin VB.Menu mnuPopAddComputer 
         Caption         =   "Add computer"
      End
      Begin VB.Menu mnuPop__ 
         Caption         =   "-"
      End
      Begin VB.Menu mnuPopSortComputers 
         Caption         =   "Sorted"
      End
   End
   Begin VB.Menu mnuPopDatabases 
      Caption         =   ""
      Visible         =   0   'False
      Begin VB.Menu mnuPopNewDatabase 
         Caption         =   "New database"
      End
      Begin VB.Menu mnuPopSortDatabases0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuPopSortDatabases 
         Caption         =   "Sorted"
      End
   End
   Begin VB.Menu mnuPopComputer 
      Caption         =   ""
      Visible         =   0   'False
      Begin VB.Menu mnuPopComputerName 
         Caption         =   "ComputerName"
         Enabled         =   0   'False
      End
      Begin VB.Menu mnuPopComputer0 
         Caption         =   "-"
      End
      Begin VB.Menu mnuPopConnectComputer 
         Caption         =   "Connect"
      End
      Begin VB.Menu mnuPopDisconnectComputer 
         Caption         =   "Disconnect"
      End
      Begin VB.Menu mnuPopRemoveComputer 
         Caption         =   "Remove"
      End
      Begin VB.Menu mnuComputer1 
         Caption         =   "-"
      End
      Begin VB.Menu mnuPopComputerProperties 
         Caption         =   "Properties"
      End
   End
End
Attribute VB_Name = "frmMain"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit
Private Declare Function OSWinHelp% Lib "user32" Alias "WinHelpA" (ByVal hwnd&, ByVal HelpFile$, ByVal wCommand%, dwData As Any)

Dim mbMoving As Boolean
Const sglSplitLimit = 500
Dim m_currentNode As MSComctlLib.Node
Dim m_currentList As ListView

Dim m_currentView As Integer
Dim m_computerWidth As Integer
Dim m_databaseWidth As Integer

Dim m_currentComputer As Computer
Dim m_currentDatabase As Database_

Private Sub Form_Load()
    tvTreeView.Nodes.Clear
    lvComputers.ListItems.Clear
    lvProcesses.ListItems.Clear
    lvDatabases.ListItems.Clear

    Me.Left = GetSetting(App.Title, "Settings", "MainLeft", 1000)
    Me.Top = GetSetting(App.Title, "Settings", "MainTop", 1000)
    Me.Width = GetSetting(App.Title, "Settings", "MainWidth", 6500)
    Me.Height = GetSetting(App.Title, "Settings", "MainHeight", 6500)
    
    tvTreeView.Nodes.Add , tvwChild, "Computers", "Computers", 1, 2
    Dim c As Computer
    For Each c In g_computers
        addComputer c
    Next
    
    Set m_currentNode = tvTreeView.Nodes("Computers")
    Set m_currentList = lvComputers
    
    tvTreeView.Nodes.Add , tvwChild, "Databases", "Databases", 1, 2
    Dim d As Database_
    For Each d In g_databases
        AddDatabase d
    Next
    
    lvComputers.Visible = True
    lvProcesses.Visible = False
    lvDatabases.Visible = False
    lvComputers.View = lvwReport
    lvProcesses.View = lvwReport
    lvDatabases.View = lvwReport
    m_computerWidth = lvProcesses.ColumnHeaders("Computer").Width
    m_databaseWidth = lvProcesses.ColumnHeaders("Database").Width
    lvProcesses.ColumnHeaders("Id").Width = 0
End Sub

Private Sub setComputer(ByVal f_ip As String)
    Dim c As Computer
    Set c = g_computers(f_ip)
    If c Is Nothing Then
        MsgBox "Unknown computer: " & f_ip
        Exit Sub
    End If
    
    Set m_currentComputer = c
    
    lblTitle(1).Caption = "Processes defined on computer: " & c.m_name
    setProcesses c.m_processes
    
    If lvProcesses.ColumnHeaders("Computer").Width <> 0 Then
        m_computerWidth = lvProcesses.ColumnHeaders("Computer").Width
        lvProcesses.ColumnHeaders("Computer").Width = 0
    End If
    
    If lvProcesses.ColumnHeaders("Database").Width = 0 Then
        lvProcesses.ColumnHeaders("Database").Width = m_databaseWidth
    End If
End Sub

Private Sub setDatabase(ByVal f_name As String)
    Dim c As Database_
    Set c = g_databases(f_name)
    If c Is Nothing Then
        MsgBox "Unknown database: " & f_name
        Exit Sub
    End If
    
    Set m_currentDatabase = c
    
    lblTitle(1).Caption = "Processes defined for database: " & c.m_name
    setProcesses c.m_processes

    If lvProcesses.ColumnHeaders("Database").Width <> 0 Then
        m_databaseWidth = lvProcesses.ColumnHeaders("Database").Width
        lvProcesses.ColumnHeaders("Database").Width = 0
    End If
    
    If lvProcesses.ColumnHeaders("Computer").Width = 0 Then
        lvProcesses.ColumnHeaders("Computer").Width = m_computerWidth
    End If

End Sub

Private Sub setProcesses(ByRef c As Collection)
    lvProcesses.ListItems.Clear
    Dim p As Process
    For Each p In c
        Dim li As ListItem
        Set li = lvProcesses.ListItems.Add(, "_" & p.m_computer.m_name & "_" & p.m_id, p.m_id)
        li.SubItems(1) = p.m_computer.m_name
        li.SubItems(2) = p.m_database
        li.SubItems(3) = p.m_name
        li.SubItems(4) = p.m_status
        li.SubItems(5) = p.m_owner
    Next
End Sub

Public Sub addComputer(ByRef c As Computer)
    Dim icon As Integer
    Select Case c.m_status
    Case "No contact"
        icon = 4
    Case "Connected"
        icon = 5
    Case Else
        icon = 3
    End Select
    
    Dim li As ListItem
    Set li = lvComputers.ListItems.Add(, "_" & c.m_name, c.m_name, icon, icon)
    li.SubItems(1) = c.m_status
    
    tvTreeView.Nodes.Add "Computers", tvwChild, "_" & c.m_name, c.m_name, icon, icon
End Sub

Public Sub removeComputer(ByRef name As String)
    lvComputers.ListItems.Remove "_" & name
    tvTreeView.Nodes.Remove "_" & name
    
    '
    ' Check if should remove database
    Dim c As Computer
    Set c = g_computers("_" & name)
    Dim db As Database_
    Dim dbs As New Collection
    Dim p As Process
    For Each p In c.m_processes
        Set db = g_databases("_" & p.m_database)
        db.m_processes.Remove "_" & p.m_computer.m_name & "_" & p.m_id
        If Not Exists(dbs, p.m_database) Then dbs.Add db, p.m_database
    Next

    For Each db In dbs
        If db.m_processes.Count = 0 Then
            g_databases.Remove "_" & db.m_name
            tvTreeView.Nodes.Remove "_" & db.m_name
        End If
    Next
        
    g_computers.Remove "_" & name
    
    '
    ' Check if should remove database
    
    Dim n As MSComctlLib.Node
    Set n = tvTreeView.SelectedItem
    selectNode n
End Sub

Private Sub AddDatabase(ByRef c As Database_)
    Dim li As ListItem
    Set li = lvDatabases.ListItems.Add(, "_" & c.m_name, c.m_name, 9, 9)
    li.SubItems(1) = c.m_status
    tvTreeView.Nodes.Add "Databases", tvwChild, "_" & c.m_name, c.m_name, 9, 9
End Sub

Private Sub Form_Unload(Cancel As Integer)
    Dim i As Integer


    'close all sub forms
    For i = Forms.Count - 1 To 1 Step -1
        Unload Forms(i)
    Next
    If Me.WindowState <> vbMinimized Then
        SaveSetting App.Title, "Settings", "MainLeft", Me.Left
        SaveSetting App.Title, "Settings", "MainTop", Me.Top
        SaveSetting App.Title, "Settings", "MainWidth", Me.Width
        SaveSetting App.Title, "Settings", "MainHeight", Me.Height
    End If
End Sub

Private Sub Form_Resize()
    On Error Resume Next
    If Me.Width < 3000 Then Me.Width = 3000
    SizeControls imgSplitter.Left
End Sub

Private Sub imgSplitter_MouseDown(Button As Integer, Shift As Integer, X As Single, Y As Single)
    With imgSplitter
        picSplitter.Move .Left, .Top, .Width \ 2, .Height - 20
    End With
    picSplitter.Visible = True
    mbMoving = True
End Sub

Private Sub imgSplitter_MouseMove(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Dim sglPos As Single
    

    If mbMoving Then
        sglPos = X + imgSplitter.Left
        If sglPos < sglSplitLimit Then
            picSplitter.Left = sglSplitLimit
        ElseIf sglPos > Me.Width - sglSplitLimit Then
            picSplitter.Left = Me.Width - sglSplitLimit
        Else
            picSplitter.Left = sglPos
        End If
    End If
End Sub


Private Sub imgSplitter_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    SizeControls picSplitter.Left
    picSplitter.Visible = False
    mbMoving = False
End Sub


Private Sub TreeView1_DragDrop(Source As Control, X As Single, Y As Single)
    If Source = imgSplitter Then
        SizeControls X
    End If
End Sub


Sub SizeControls(X As Single)
    On Error Resume Next
    
    'set the width
    If X < 1500 Then X = 1500
    If X > (Me.Width - 1500) Then X = Me.Width - 1500
    tvTreeView.Width = X
    imgSplitter.Left = X
    
    Dim t_left, t_width As Integer
    t_left = X + 40
    t_width = Me.Width - (tvTreeView.Width + 140)
    
    lblTitle(0).Width = tvTreeView.Width
    lblTitle(1).Left = t_left + 20
    lblTitle(1).Width = t_width - 40


    'set the top
    If tbToolBar.Visible Then
        tvTreeView.Top = tbToolBar.Height + picTitles.Height
    Else
        tvTreeView.Top = picTitles.Height
    End If

    
    'set the height
    If sbStatusBar.Visible Then
        tvTreeView.Height = Me.ScaleHeight - (picTitles.Top + picTitles.Height + sbStatusBar.Height)
    Else
        tvTreeView.Height = Me.ScaleHeight - (picTitles.Top + picTitles.Height)
    End If
    

    imgSplitter.Top = tvTreeView.Top
    imgSplitter.Height = tvTreeView.Height
    
    setListDimensions t_left, t_width, tvTreeView.Top, tvTreeView.Height
End Sub

Private Sub setListView(ByVal f_View As Integer)
    lvComputers.View = f_View
    lvProcesses.View = f_View
End Sub

Private Sub setListDimensions(ByVal f_Left As Integer, ByVal f_Width As Integer, ByVal f_Top As Integer, ByVal f_Height As Integer)
    With lvComputers
        .Left = f_Left
        .Width = f_Width
        .Top = f_Top
        .Height = f_Height
    End With
    With lvProcesses
        .Left = f_Left
        .Width = f_Width
        .Top = f_Top
        .Height = f_Height
    End With
    With lvDatabases
        .Left = f_Left
        .Width = f_Width
        .Top = f_Top
        .Height = f_Height
    End With
End Sub

Private Sub tbToolBar_ButtonClick(ByVal Button As MSComctlLib.Button)
    On Error Resume Next
    Select Case Button.Key
        Case "New database"
            'ToDo: Add 'Back' button code.
            mnuPopNewDatabase_Click
        Case "Add computer"
            'ToDo: Add 'Forward' button code.
            frmNewComputer.Show vbModal, Me
            Dim c As Computer
            For Each c In frmNewComputer.m_hosts
                addComputer c
                g_computers.Add c, "_" & c.m_name
            Next
        Case "Properties"
            mnuFileProperties_Click
    End Select
End Sub

Private Sub mnuHelpAbout_Click()
    frmAbout.Show vbModal, Me
End Sub

Private Sub mnuHelpSearchForHelpOn_Click()
    Dim nRet As Integer


    'if there is no helpfile for this project display a message to the user
    'you can set the HelpFile for your application in the
    'Project Properties dialog
    If Len(App.HelpFile) = 0 Then
        MsgBox "Unable to display Help Contents. There is no Help associated with this project.", vbInformation, Me.Caption
    Else
        On Error Resume Next
        nRet = OSWinHelp(Me.hwnd, App.HelpFile, 261, 0)
        If Err Then
            MsgBox Err.Description
        End If
    End If

End Sub

Private Sub mnuHelpContents_Click()
    Dim nRet As Integer


    'if there is no helpfile for this project display a message to the user
    'you can set the HelpFile for your application in the
    'Project Properties dialog
    If Len(App.HelpFile) = 0 Then
        MsgBox "Unable to display Help Contents. There is no Help associated with this project.", vbInformation, Me.Caption
    Else
        On Error Resume Next
        nRet = OSWinHelp(Me.hwnd, App.HelpFile, 3, 0)
        If Err Then
            MsgBox Err.Description
        End If
    End If

End Sub


Private Sub mnuViewWebBrowser_Click()
    'ToDo: Add 'mnuViewWebBrowser_Click' code.
    MsgBox "Add 'mnuViewWebBrowser_Click' code."
End Sub

Private Sub mnuViewOptions_Click()
    frmOptions.Show vbModal, Me
End Sub

Private Sub mnuViewRefresh_Click()
    'ToDo: Add 'mnuViewRefresh_Click' code.
    MsgBox "Add 'mnuViewRefresh_Click' code."
End Sub


Private Sub mnuViewStatusBar_Click()
    mnuViewStatusBar.Checked = Not mnuViewStatusBar.Checked
    sbStatusBar.Visible = mnuViewStatusBar.Checked
    SizeControls imgSplitter.Left
End Sub

Private Sub mnuViewToolbar_Click()
    mnuViewToolbar.Checked = Not mnuViewToolbar.Checked
    tbToolBar.Visible = mnuViewToolbar.Checked
    SizeControls imgSplitter.Left
End Sub

Private Sub mnuEditInvertSelection_Click()
    'ToDo: Add 'mnuEditInvertSelection_Click' code.
    MsgBox "Add 'mnuEditInvertSelection_Click' code."
End Sub

Private Sub mnuEditSelectAll_Click()
    'ToDo: Add 'mnuEditSelectAll_Click' code.
    MsgBox "Add 'mnuEditSelectAll_Click' code."
End Sub

Private Sub mnuEditPasteSpecial_Click()
    'ToDo: Add 'mnuEditPasteSpecial_Click' code.
    MsgBox "Add 'mnuEditPasteSpecial_Click' code."
End Sub

Private Sub mnuEditPaste_Click()
    'ToDo: Add 'mnuEditPaste_Click' code.
    MsgBox "Add 'mnuEditPaste_Click' code."
End Sub

Private Sub mnuEditCopy_Click()
    'ToDo: Add 'mnuEditCopy_Click' code.
    MsgBox "Add 'mnuEditCopy_Click' code."
End Sub

Private Sub mnuEditCut_Click()
    'ToDo: Add 'mnuEditCut_Click' code.
    MsgBox "Add 'mnuEditCut_Click' code."
End Sub

Private Sub mnuEditUndo_Click()
    'ToDo: Add 'mnuEditUndo_Click' code.
    MsgBox "Add 'mnuEditUndo_Click' code."
End Sub

Private Sub mnuFileClose_Click()
    'unload the form
    Unload Me

End Sub

Private Sub mnuFileProperties_Click()
    'ToDo: Add 'mnuFileProperties_Click' code.
    MsgBox "Add 'mnuFileProperties_Click' code."
End Sub

Private Sub mnuFileRename_Click()
    'ToDo: Add 'mnuFileRename_Click' code.
    MsgBox "Add 'mnuFileRename_Click' code."
End Sub

Private Sub mnuFileDelete_Click()
    'ToDo: Add 'mnuFileDelete_Click' code.
    MsgBox "Add 'mnuFileDelete_Click' code."
End Sub

Private Sub mnuFileNew_Click()
    'ToDo: Add 'mnuFileNew_Click' code.
    MsgBox "Add 'mnuFileNew_Click' code."
End Sub

Private Sub mnuFileSendTo_Click()
    'ToDo: Add 'mnuFileSendTo_Click' code.
    MsgBox "Add 'mnuFileSendTo_Click' code."
End Sub

Private Sub mnuFileFind_Click()
    'ToDo: Add 'mnuFileFind_Click' code.
    MsgBox "Add 'mnuFileFind_Click' code."
End Sub

Private Sub mnuFileOpen_Click()
    Dim sFile As String
End Sub

Private Sub mnuPopComputerProperties_Click()
    mnuFileProperties_Click
End Sub

Private Sub mnuPopNewDatabase_Click()
    frmNewDatabase1.Show vbModal, Me
    frmNewDatabase2.Show vbModal, Me
    frmNewDatabase3.Show vbModal, Me
End Sub

Private Sub mnuPopAddComputer_Click()
    frmNewComputer.Show vbModal, Me
    Dim c As Computer
    For Each c In frmNewComputer.m_hosts
        addComputer c
        g_computers.Add c, "_" & c.m_name
    Next
End Sub

Private Sub mnuPopSortComputers_Click()
    If m_currentNode.Sorted = True Then
        mnuPopSortComputers.Checked = False
        m_currentNode.Sorted = False
    Else
        mnuPopSortComputers.Checked = True
        m_currentNode.Sorted = True
    End If
End Sub

Private Sub mnuPopRemoveComputer_Click()
    Dim res As VbMsgBoxResult
    Dim str As String
    str = "Remove computer " & m_currentComputer.m_name
    res = MsgBox(str, vbOKCancel, str)
    If res = vbOK Then
        removeComputer (m_currentComputer.m_name)
    End If
End Sub

Private Sub mnuPopSortDatabases_Click()
    If m_currentNode.Sorted = True Then
        mnuPopSortDatabases.Checked = False
        m_currentNode.Sorted = False
    Else
        mnuPopSortDatabases.Checked = True
        m_currentNode.Sorted = True
    End If
End Sub

Private Sub tvTreeView_BeforeLabelEdit(Cancel As Integer)
    Cancel = True
End Sub

Private Sub tvTreeView_Collapse(ByVal Node As MSComctlLib.Node)
    'MsgBox "tvTreeView_Collapse"
End Sub

Private Sub tvTreeView_Expand(ByVal Node As MSComctlLib.Node)
    'MsgBox "tvTreeView_Expand"
End Sub

Private Sub tvTreeView_NodeClick(ByVal Node As MSComctlLib.Node)
    selectNode Node
End Sub

Private Sub tvTreeView_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    'MsgBox "tvTreeView_MouseUp Button: " & Button & " Shift: " & Shift
    Dim Node As MSComctlLib.Node
    Dim place As Integer
    
    Set Node = tvTreeView.HitTest(X, Y)
    place = selectNode(Node)
    If Button = vbRightButton Then
        ShowPopup place
    End If
End Sub

Private Function selectNode(ByRef n As MSComctlLib.Node) As Integer
    Dim list As ListView
    Dim place As Integer
    
    If n Is Nothing Then
        If Not m_currentNode Is Nothing Then
            place = 1
            m_currentNode.Selected = False
        Else
            place = 2
        End If
    Else
        n.Selected = True
        If n.Text = "Computers" Then
            place = 3
            Set list = lvComputers
            lblTitle(1).Caption = "Computers"
        ElseIf n.Text = "Databases" Then
            place = 4
            Set list = lvDatabases
            lblTitle(1).Caption = "Databases"
        ElseIf n.Parent.Text = "Computers" Then
            place = 5
            Set list = lvProcesses
            setComputer (n.Key)
        ElseIf n.Parent.Text = "Databases" Then
            place = 6
            Set list = lvProcesses
            setDatabase (n.Key)
        End If
    
        If m_currentList.hwnd <> list.hwnd Then
            m_currentList.Visible = False
            list.Visible = True
            Set m_currentList = list
        End If
    End If
    Set m_currentNode = n
    selectNode = place
End Function

Private Sub lvComputers_MouseUp(Button As Integer, Shift As Integer, X As Single, Y As Single)
    Dim li As ListItem
    Set li = lvComputers.HitTest(X, Y)
    If Button = vbRightButton And Not li Is Nothing Then
        Dim c As Computer
        Set m_currentComputer = g_computers(li.Key)
        ShowPopup 5
    End If
End Sub

Private Sub ShowPopup(ByVal place As Integer)
    Select Case place
    Case 3
        PopupMenu mnuPopComputers
    Case 4
        PopupMenu mnuPopDatabases
    Case 5
        mnuPopComputerName.Caption = m_currentComputer.m_name & ": " & m_currentComputer.m_status
        Select Case m_currentComputer.m_status
        Case "Connected"
            mnuPopConnectComputer.Enabled = False
            mnuPopDisconnectComputer.Enabled = True
        Case "Connecting"
            mnuPopConnectComputer.Enabled = False
            mnuPopDisconnectComputer.Enabled = True
        Case "Not connected"
            mnuPopConnectComputer.Enabled = True
            mnuPopDisconnectComputer.Enabled = False
        Case "No contact"
            mnuPopConnectComputer.Enabled = True
            mnuPopDisconnectComputer.Enabled = False
        Case Else
            mnuPopConnectComputer.Enabled = False
            mnuPopDisconnectComputer.Enabled = False
        End Select
        
        PopupMenu mnuPopComputer, , , , mnuPopComputerName
    End Select
End Sub

Private Sub lvComputers_BeforeLabelEdit(Cancel As Integer)
    Cancel = True
End Sub

Private Sub lvProcesses_BeforeLabelEdit(Cancel As Integer)
    Cancel = True
End Sub

Private Sub lvDatabases_BeforeLabelEdit(Cancel As Integer)
    Cancel = True
End Sub

Private Sub ColumnClick(ByRef list As ListView, i As Integer)
    i = i - 1
    If list.SortKey = i Then
        list.SortOrder = 1 - list.SortOrder
    Else
        list.SortKey = i
    End If
End Sub

Private Sub lvComputers_ColumnClick(ByVal ColumnHeader As MSComctlLib.ColumnHeader)
    ColumnClick lvComputers, ColumnHeader.Index
End Sub

Private Sub lvProcesses_ColumnClick(ByVal ColumnHeader As MSComctlLib.ColumnHeader)
    ColumnClick lvProcesses, ColumnHeader.Index
End Sub

Private Sub lvDatabases_ColumnClick(ByVal ColumnHeader As MSComctlLib.ColumnHeader)
    ColumnClick lvDatabases, ColumnHeader.Index
End Sub

