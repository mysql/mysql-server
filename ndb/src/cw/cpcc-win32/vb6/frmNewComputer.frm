VERSION 5.00
Begin VB.Form frmNewComputer 
   Caption         =   "Add computer"
   ClientHeight    =   1545
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   4605
   LinkTopic       =   "Form1"
   ScaleHeight     =   1545
   ScaleWidth      =   4605
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton Command3 
      Caption         =   "Apply"
      Default         =   -1  'True
      Height          =   360
      Left            =   3240
      TabIndex        =   4
      Tag             =   "OK"
      Top             =   840
      Width           =   1140
   End
   Begin VB.CommandButton Command2 
      Caption         =   "Cancel"
      Height          =   360
      Left            =   1920
      TabIndex        =   3
      Tag             =   "OK"
      Top             =   840
      Width           =   1140
   End
   Begin VB.CommandButton Command1 
      Caption         =   "OK"
      Height          =   360
      Left            =   600
      TabIndex        =   2
      Tag             =   "OK"
      Top             =   840
      Width           =   1140
   End
   Begin VB.TextBox Text1 
      Height          =   285
      Left            =   1440
      TabIndex        =   1
      Top             =   240
      Width           =   2925
   End
   Begin VB.Label lblLabels 
      Caption         =   "Computer name:"
      Height          =   255
      Index           =   1
      Left            =   120
      TabIndex        =   0
      Tag             =   "&User Name:"
      Top             =   240
      Width           =   1440
   End
End
Attribute VB_Name = "frmNewComputer"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Public m_hosts As New Collection

Private Sub Form_Load()
    If m_hosts.Count > 0 Then
        For i = m_hosts.Count To 1 Step -1
            m_hosts.Remove i
        Next
    End If
End Sub

Private Sub Command1_Click()
    If Text1.Text = "" Then
        MsgBox "Invalid hostname"
        Exit Sub
    End If
    
    If Exists(g_computers, "_" & Text1.Text) Then
        MsgBox Text1.Text & " already exists"
        Exit Sub
    End If
    
    Dim c As New Computer
    With c
        .m_ip = ""
        .m_name = Text1.Text
        .m_status = "Not connected"
        Set .m_processes = New Collection
    End With
    
    m_hosts.Add c

    Unload Me
End Sub

Private Sub Command2_Click()
    Unload Me
End Sub

Private Sub Command3_Click()
    If Text1.Text = "" Then
        MsgBox "Invalid hostname"
        Exit Sub
    End If
    
    If Exists(g_computers, "_" & Text1.Text) Then
        MsgBox Text1.Text & " already exists"
        Exit Sub
    End If
    
    Dim c As New Computer
    With c
        .m_ip = ""
        .m_name = Text1.Text
        .m_status = "Not connected"
        Set .m_processes = New Collection
    End With
    
    m_hosts.Add c

    Text1.Text = ""
End Sub

