' =====================================================
' ESP32 MUD File Upload - VB.NET Usage Examples
' =====================================================

' Example 1: Simple single file upload with error handling
Public Sub UploadSingleFile()
    Dim uploader As New ESP32FileUploader("192.168.1.100")  ' Change to your device IP
    
    ' Test connection first
    If Not uploader.TestConnection() Then
        MessageBox.Show("Cannot reach device at 192.168.1.100:8080", "Connection Failed")
        Return
    End If
    
    ' Upload a file
    Dim result = uploader.UploadFile("C:\path\to\shops.txt")
    
    If result Then
        MessageBox.Show("File uploaded successfully!")
    Else
        MessageBox.Show("Upload failed!")
    End If
End Sub


' Example 2: Upload with progress reporting
Public Sub UploadWithProgress()
    Dim uploader As New ESP32FileUploader("192.168.1.100")
    
    ' Define progress callback
    Dim progressHandler As Action(Of String) = Sub(message)
                                                   Me.txtStatus.Text = message
                                                   Me.txtStatus.Refresh()
                                               End Sub
    
    ' Upload file with progress
    Dim result = uploader.UploadFile("C:\path\to\items.vxd", progressHandler)
End Sub


' Example 3: Upload entire data folder
Public Sub UploadDataFolder()
    Dim uploader As New ESP32FileUploader("192.168.1.100")
    
    ' Define progress callback for multi-file upload
    Dim progressHandler As Action(Of String) = Sub(message)
                                                   Me.lstStatus.Items.Add(message)
                                                   Me.lstStatus.SelectedIndex = Me.lstStatus.Items.Count - 1
                                               End Sub
    
    ' Upload all files from data folder
    Dim dataFolder = Application.StartupPath & "\data"
    Dim successCount = uploader.UploadFolder(dataFolder, progressHandler)
    
    MessageBox.Show($"Upload complete! {successCount} files uploaded successfully.")
End Sub


' Example 4: Batch upload with UI form
Public Class UploadForm
    Inherits Form
    
    Private WithEvents btnBrowse As Button
    Private WithEvents btnUpload As Button
    Private WithEvents txtDeviceIP As TextBox
    Private WithEvents lstFiles As ListBox
    Private WithEvents lstStatus As ListBox
    Private WithEvents lblDeviceIP As Label
    
    Public Sub New()
        InitializeComponent()
    End Sub
    
    Private Sub InitializeComponent()
        ' UI Setup code
        Me.Text = "ESP32 MUD File Uploader"
        Me.Size = New Size(600, 500)
        
        ' Device IP Input
        lblDeviceIP = New Label With {.Text = "Device IP:", .Location = New Point(10, 10), .Size = New Size(100, 20)}
        Me.Controls.Add(lblDeviceIP)
        
        txtDeviceIP = New TextBox With {.Text = "192.168.1.100", .Location = New Point(110, 10), .Size = New Size(150, 20)}
        Me.Controls.Add(txtDeviceIP)
        
        ' Browse Button
        btnBrowse = New Button With {.Text = "Browse Files", .Location = New Point(270, 10), .Size = New Size(100, 20)}
        Me.Controls.Add(btnBrowse)
        
        ' Files ListBox
        lstFiles = New ListBox With {.Location = New Point(10, 40), .Size = New Size(360, 200)}
        Me.Controls.Add(lstFiles)
        
        ' Status ListBox
        lstStatus = New ListBox With {.Location = New Point(10, 250), .Size = New Size(360, 150)}
        Me.Controls.Add(lstStatus)
        
        ' Upload Button
        btnUpload = New Button With {.Text = "Upload All", .Location = New Point(10, 410), .Size = New Size(100, 30)}
        Me.Controls.Add(btnUpload)
    End Sub
    
    Private Sub btnBrowse_Click(sender As Object, e As EventArgs) Handles btnBrowse.Click
        Dim dlg As New FolderBrowserDialog()
        If dlg.ShowDialog() = DialogResult.OK Then
            lstFiles.Items.Clear()
            For Each file In Directory.GetFiles(dlg.SelectedPath)
                lstFiles.Items.Add(Path.GetFileName(file))
            Next
        End If
    End Sub
    
    Private Sub btnUpload_Click(sender As Object, e As EventArgs) Handles btnUpload.Click
        Dim deviceIP = txtDeviceIP.Text.Trim()
        If String.IsNullOrEmpty(deviceIP) Then
            MessageBox.Show("Please enter device IP address")
            Return
        End If
        
        Dim uploader As New ESP32FileUploader(deviceIP)
        
        ' Test connection
        lstStatus.Items.Add("Testing connection...")
        lstStatus.Refresh()
        
        If Not uploader.TestConnection() Then
            lstStatus.Items.Add("✗ Cannot reach device!")
            Return
        End If
        
        lstStatus.Items.Add("✓ Connected to device")
        lstStatus.Items.Add("")
        
        ' Progress callback
        Dim progressHandler As Action(Of String) = Sub(message)
                                                       lstStatus.Items.Add(message)
                                                       lstStatus.SelectedIndex = lstStatus.Items.Count - 1
                                                       lstStatus.Refresh()
                                                   End Sub
        
        ' Upload each file
        For Each item In lstFiles.Items
            Dim filePath = Path.Combine(Path.GetDirectoryName(lstFiles.Items(0).ToString()), item.ToString())
            uploader.UploadFile(filePath, progressHandler)
            Threading.Thread.Sleep(500)
        Next
        
        lstStatus.Items.Add("")
        lstStatus.Items.Add("Upload complete!")
    End Sub
End Class
