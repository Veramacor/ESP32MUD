' Example: How to use WebsiteFileUploader in your VB.NET mapper

Imports System.IO

' ============================================================
' EXAMPLE 1: Upload a single file
' ============================================================
Sub UploadSingleFile()
    Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
    
    Dim filePath = "C:\path\to\your\file.vxd"
    Dim remoteFileName = "items.vxd"
    
    If uploader.UploadFile(filePath, remoteFileName, 
            Sub(msg) Console.WriteLine(msg)) Then
        Console.WriteLine("File uploaded successfully!")
    Else
        Console.WriteLine("File upload failed!")
    End If
    
    uploader.Dispose()
End Sub

' ============================================================
' EXAMPLE 2: Upload multiple files from a List
' ============================================================
Sub UploadMultipleFiles()
    Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
    
    ' Create list of files to upload
    Dim filesToUpload As New List(Of String) From {
        "C:\data\items.vxd",
        "C:\data\rooms.txt",
        "C:\data\npcs.txt"
    }
    
    Dim successCount = uploader.UploadFiles(filesToUpload,
        Sub(msg) Console.WriteLine(msg))
    
    Console.WriteLine($"Successfully uploaded {successCount} files")
    uploader.Dispose()
End Sub

' ============================================================
' EXAMPLE 3: Upload with custom remote filenames (Dictionary)
' ============================================================
Sub UploadWithCustomNames()
    Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
    
    ' Map local files to remote filenames
    Dim fileMapping As New Dictionary(Of String, String) From {
        {"C:\local\myitems.vxd", "items.vxd"},
        {"C:\local\myrooms.txt", "rooms.txt"},
        {"C:\local\mynpcs.txt", "npcs.txt"}
    }
    
    Dim successCount = uploader.UploadFilesWithMapping(fileMapping,
        Sub(msg) 
            ' This callback gets called for each status update
            Console.WriteLine(msg)
        End Sub)
    
    Console.WriteLine($"Uploaded {successCount}/{fileMapping.Count} files")
    uploader.Dispose()
End Sub

' ============================================================
' EXAMPLE 4: Integrate with your existing mapper file list
' ============================================================
Sub UploadMapperFiles(fileList As List(Of String))
    ' Assuming fileList is already populated with file paths
    
    Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
    
    ' Upload all files in the list
    Dim successCount = uploader.UploadFiles(fileList,
        Sub(msg)
            ' Update your UI with progress
            Console.WriteLine(msg)
            ' Or call a UI update method:
            ' UpdateMapperStatus(msg)
        End Sub)
    
    If successCount = fileList.Count Then
        Console.WriteLine("All files uploaded successfully!")
    Else
        Console.WriteLine($"Warning: Only {successCount}/{fileList.Count} files uploaded successfully")
    End If
    
    uploader.Dispose()
End Sub

' ============================================================
' EXAMPLE 5: Button click handler with Results List (WinForms)
' ============================================================
Private Sub btnUploadToWebsite_Click(sender As Object, e As EventArgs) Handles btnUploadToWebsite.Click
    Try
        ' Create results list for displaying messages
        Dim results As New List(Of String)
        
        ' Disable button while uploading
        btnUploadToWebsite.Enabled = False
        
        ' Your upload list with tuple structure
        Dim uploadList As New List(Of (LocalPath As String, RemoteName As String)) From {
            (localRooms, "rooms.txt"),
            (localItemsVxd, "items.vxd"),
            (localItemsVxi, "items.vxi"),
            (localNpcsVxd, "npcs.vxd"),
            (localNpcsVxi, "npcs.vxi"),
            (localQuestsTxt, "quests.txt")
        }
        
        ' Create uploader and upload files
        Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
        
        ' Upload with callback that adds to results list
        Dim successCount = uploader.UploadFileList(uploadList,
            Sub(msg)
                ' Add message to results list
                results.Add(msg)
                ' Also update UI in real-time
                lstResults.Items.Add(msg)
                lstResults.TopIndex = lstResults.Items.Count - 1  ' Scroll to latest
            End Sub)
        
        ' Add summary message
        results.Add("")
        results.Add($"========== UPLOAD SUMMARY ==========")
        results.Add($"Successfully uploaded: {successCount}/{uploadList.Count} files")
        
        lstResults.Items.Add("")
        lstResults.Items.Add($"========== UPLOAD SUMMARY ==========")
        lstResults.Items.Add($"Successfully uploaded: {successCount}/{uploadList.Count} files")
        
        ' Show result dialog
        MsgBox($"Upload complete: {successCount}/{uploadList.Count} files successful", 
               MsgBoxStyle.Information, "Upload Result")
        
        uploader.Dispose()
        
    Catch ex As Exception
        MsgBox($"Error during upload: {ex.Message}", 
               MsgBoxStyle.Critical, "Upload Error")
    Finally
        btnUploadToWebsite.Enabled = True
    End Try
End Sub

' ============================================================
' EXAMPLE 6: Alternative - Collect results, display after
' ============================================================
Private Sub btnUploadAlternative_Click(sender As Object, e As EventArgs)
    Try
        ' Create results list (will collect all messages)
        Dim results As New List(Of String)
        
        btnUploadAlternative.Enabled = False
        
        ' Your upload list
        Dim uploadList As New List(Of (LocalPath As String, RemoteName As String)) From {
            (localRooms, "rooms.txt"),
            (localItemsVxd, "items.vxd"),
            (localItemsVxi, "items.vxi"),
            (localNpcsVxd, "npcs.vxd"),
            (localNpcsVxi, "npcs.vxi"),
            (localQuestsTxt, "quests.txt")
        }
        
        ' Upload and collect all messages in results list
        Dim uploader = New WebsiteFileUploader("https://www.storyboardacs.com")
        Dim successCount = uploader.UploadFileList(uploadList,
            Sub(msg) results.Add(msg))  ' Just add to list, no UI updates
        
        uploader.Dispose()
        
        ' Now display all results in one go
        lstResults.Items.Clear()
        For Each msg In results
            lstResults.Items.Add(msg)
        Next
        
        ' Add summary
        lstResults.Items.Add("")
        lstResults.Items.Add($"========== UPLOAD SUMMARY ==========")
        lstResults.Items.Add($"Successfully uploaded: {successCount}/{uploadList.Count} files")
        lstResults.Items.Add($"Timestamp: {DateTime.Now}")
        
        ' Show final result
        MsgBox($"Upload complete: {successCount}/{uploadList.Count} files successful", 
               MsgBoxStyle.Information, "Upload Result")
        
    Catch ex As Exception
        MsgBox($"Error during upload: {ex.Message}", 
               MsgBoxStyle.Critical, "Upload Error")
    Finally
        btnUploadAlternative.Enabled = True
    End Try
End Sub

' ============================================================
' USAGE NOTES:
' ============================================================
' 1. Add WebsiteFileUploader.vb to your VB.NET project
' 2. Place upload.php on your web server at https://www.storyboardacs.com/upload.php
' 3. Ensure /mud_files directory is writable by the web server:
'    - Create directory: mkdir -p /home/user/public_html/mud_files
'    - Set permissions: chmod 755 /home/user/public_html/mud_files
' 4. The uploader will overwrite existing files on the server
' 5. Always call Dispose() when done to clean up resources
' 6. Use Try/Catch for production code to handle network errors
