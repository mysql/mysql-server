VERSION 5.00
Begin VB.Form frmNewDatabase3 
   BorderStyle     =   5  'Sizable ToolWindow
   Caption         =   "Database configuration"
   ClientHeight    =   3000
   ClientLeft      =   2850
   ClientTop       =   3450
   ClientWidth     =   6240
   LinkTopic       =   "Form1"
   MaxButton       =   0   'False
   MinButton       =   0   'False
   ScaleHeight     =   3281.25
   ScaleMode       =   0  'User
   ScaleWidth      =   6359.712
   ShowInTaskbar   =   0   'False
   StartUpPosition =   2  'CenterScreen
   Begin VB.CommandButton cmdCancel 
      Cancel          =   -1  'True
      Caption         =   "Cancel"
      Height          =   305
      Left            =   1320
      TabIndex        =   3
      Top             =   2400
      Width           =   1140
   End
   Begin VB.CommandButton cmdFinish 
      Caption         =   "Finish"
      Enabled         =   0   'False
      Height          =   305
      Left            =   5040
      TabIndex        =   2
      Top             =   2400
      Width           =   1140
   End
   Begin VB.CommandButton cmdBack 
      Caption         =   "Back"
      Default         =   -1  'True
      Enabled         =   0   'False
      Height          =   305
      Left            =   2640
      TabIndex        =   0
      Top             =   2400
      Width           =   1140
   End
   Begin VB.CommandButton cmdNext 
      Caption         =   "Next"
      Height          =   305
      Left            =   3720
      TabIndex        =   1
      Top             =   2400
      Width           =   1140
   End
   Begin VB.Line Line1 
      BorderColor     =   &H80000003&
      X1              =   122.302
      X2              =   6237.41
      Y1              =   2493.75
      Y2              =   2493.75
   End
End
Attribute VB_Name = "frmNewDatabase3"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

Private Sub Form_Resize()
    If Me.Width < 6375 Then Me.Width = 6375
    cmdCancel.Left = Me.ScaleWidth - 5136 + 400
    cmdBack.Left = Me.ScaleWidth - 3897 + 400
    cmdNext.Left = Me.ScaleWidth - 2883 + 400
    cmdFinish.Left = Me.ScaleWidth - 1643 + 400
    Line1.X2 = Me.ScaleWidth - 480 + 400
    
    cmdCancel.Top = Me.ScaleHeight - 375
    cmdBack.Top = Me.ScaleHeight - 375
    cmdNext.Top = Me.ScaleHeight - 375
    cmdFinish.Top = Me.ScaleHeight - 375
    Line1.Y1 = Me.ScaleHeight - 475
    Line1.Y2 = Me.ScaleHeight - 475
End Sub

Private Sub cmdCancel_Click()
    'set the global var to false
    'to denote a failed login
    Unload Me
End Sub
