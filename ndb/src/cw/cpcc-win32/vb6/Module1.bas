Attribute VB_Name = "Module1"
Option Explicit
Public fMainForm As frmMain
Public g_computers As New Collection
Public g_databases As New Collection

Sub Main()
    If False Then
        Dim fLogin As New frmLogin
        fLogin.Show vbModal
        If Not fLogin.OK Then
            'Login Failed so exit app
            End
        End If
        Unload fLogin

        frmSplash.Show
        frmSplash.Refresh
    End If
    
    init
    
    Set fMainForm = New frmMain
    Load fMainForm
    Unload frmSplash

    fMainForm.Show
End Sub

Private Sub init()
    Dim c As Computer
    Dim p As Process
        
    ' ---
    ' One node configuration
    '
    Set c = New Computer
    With c
        .m_ip = "130.100.232.31"
        .m_name = "ndb-client31"
        .m_status = "Connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set p = New Process
    With p
        .m_id = "1"
        .m_name = "mgm-1"
        .m_database = "elathal"
        .m_status = "Running"
        .m_owner = "elathal"
        Set .m_computer = c
    End With
    addProcess c, p
        
    Set p = New Process
    With p
        .m_id = "2"
        .m_name = "ndb-2"
        .m_database = "elathal"
        .m_status = "Running"
        .m_owner = "elathal"
        Set .m_computer = c
    End With
    addProcess c, p

    Set p = New Process
    With p
        .m_id = "3"
        .m_name = "api-3"
        .m_database = "elathal"
        .m_status = "Running"
        .m_owner = "elathal"
        Set .m_computer = c
    End With
    addProcess c, p

    ' ---
    ' Two node configuration
    '
    Set p = New Process
    With p
        .m_id = "4"
        .m_name = "mgm-1"
        .m_database = "ejonore-2-node"
        .m_status = "Running"
        .m_owner = "ejonore"
        Set .m_computer = c
    End With
    addProcess c, p
        
    Set c = New Computer
    With c
        .m_ip = "10.0.1.1"
        .m_name = "cluster-1"
        .m_status = "Connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set p = New Process
    With p
        .m_id = "1"
        .m_name = "ndb-2"
        .m_database = "ejonore-2-node"
        .m_status = "Running"
        .m_owner = "ejonore"
        Set .m_computer = c
    End With
    addProcess c, p

    Set c = New Computer
    With c
        .m_ip = "10.0.2.1"
        .m_name = "cluster-2"
        .m_status = "Connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set p = New Process
    With p
        .m_id = "1"
        .m_name = "ndb-3"
        .m_database = "ejonore-2-node"
        .m_status = "Running"
        .m_owner = "ejonore"
        Set .m_computer = c
    End With
    addProcess c, p
    
    Set c = New Computer
    With c
        .m_ip = "10.0.3.1"
        .m_name = "cluster-3"
        .m_status = "Connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set p = New Process
    With p
        .m_id = "1"
        .m_name = "api-4"
        .m_database = "ejonore-2-node"
        .m_status = "Running"
        .m_owner = "ejonore"
        Set .m_computer = c
    End With
    addProcess c, p
    
    Set c = New Computer
    With c
        .m_ip = "10.0.4.1"
        .m_name = "cluster-4"
        .m_status = "Connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set p = New Process
    With p
        .m_id = "1"
        .m_name = "api-5"
        .m_database = "ejonore-2-node"
        .m_status = "Running"
        .m_owner = "ejonore"
        Set .m_computer = c
    End With
    addProcess c, p
    
    Set c = New Computer
    With c
        .m_ip = "130.100.232.5"
        .m_name = "ndbs05"
        .m_status = "Not connected"
        Set .m_processes = New Collection
    End With
    addComputer c
    
    Set c = New Computer
    With c
        .m_ip = "130.100.232.7"
        .m_name = "ndb-srv7"
        .m_status = "No contact"
        Set .m_processes = New Collection
    End With
    addComputer c
    
End Sub

Public Sub addComputer(ByRef c As Computer)
    g_computers.Add c, "_" & c.m_name
End Sub

Private Sub addProcess(ByRef c As Computer, ByRef p As Process)
    c.m_processes.Add p, "_" & p.m_id
        
    Dim cl As Database_
    If Not Exists(g_databases, "_" & p.m_database) Then
        Set cl = New Database_
        With cl
            .m_name = p.m_database
            .m_status = "Unknown"
            Set .m_processes = New Collection
        End With
        g_databases.Add cl, "_" & p.m_database
    Else
        Set cl = g_databases("_" & p.m_database)
    End If
    cl.m_processes.Add p, "_" & p.m_computer.m_name & "_" & p.m_id
End Sub

Public Function Exists(ByRef c As Collection, ByVal k As String) As Boolean
    Dim r As Boolean
    Dim o As Object
    
    r = True
    
    On Error GoTo NotFound
    Set o = c.Item(k)
    GoTo Continue
NotFound:
    If Err.Number <> 5 Then
        Err.Raise Err.Number, Err.Source, Err.Description
    End If
    
    r = False
Continue:
    Exists = r
End Function

