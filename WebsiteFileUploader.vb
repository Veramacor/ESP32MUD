Imports System.Net.Http
Imports System.IO

''' <summary>
''' Website File Upload Utility
''' Provides methods to upload files to storyboardacs.com/upload.php
''' </summary>
Public Class WebsiteFileUploader

    Private ReadOnly WebsiteUrl As String
    Private ReadOnly HttpClient As HttpClient

    ''' <summary>
    ''' Initialize with website base URL
    ''' </summary>
    Public Sub New(websiteUrl As String)
        ' Ensure URL ends without trailing slash for consistency
        Me.WebsiteUrl = websiteUrl.TrimEnd("/"c)
        Me.HttpClient = New HttpClient()
    End Sub

    ''' <summary>
    ''' Upload a single file to the website
    ''' </summary>
    ''' <param name="localFilePath">Full path to the local file to upload</param>
    ''' <param name="remoteFileName">Name of the file on the server (optional, defaults to local filename)</param>
    ''' <returns>True if successful, False otherwise</returns>
    Public Function UploadFile(localFilePath As String, Optional remoteFileName As String = "") As Boolean
        Return UploadFile(localFilePath, remoteFileName, Nothing)
    End Function

    ''' <summary>
    ''' Upload a single file with progress callback
    ''' </summary>
    ''' <param name="localFilePath">Full path to the local file to upload</param>
    ''' <param name="remoteFileName">Name of the file on the server</param>
    ''' <param name="statusCallback">Optional callback for status updates</param>
    ''' <returns>True if successful, False otherwise</returns>
    Public Function UploadFile(localFilePath As String, remoteFileName As String, statusCallback As Action(Of String)) As Boolean
        Try
            If Not File.Exists(localFilePath) Then
                statusCallback?.Invoke($"ERROR: File not found: {localFilePath}")
                Return False
            End If

            ' Use local filename if remote name not specified
            If String.IsNullOrEmpty(remoteFileName) Then
                remoteFileName = Path.GetFileName(localFilePath)
            End If

            Dim fileSize = New FileInfo(localFilePath).Length
            statusCallback?.Invoke($"Uploading {remoteFileName} ({fileSize} bytes)...")

            ' Read file content
            Dim fileContent = File.ReadAllBytes(localFilePath)

            ' Build upload URL with filename parameter
            Dim uploadUrl = $"{WebsiteUrl}/upload.php?file={Uri.EscapeDataString(remoteFileName)}"

            ' Create HTTP request with file content
            Using content As New ByteArrayContent(fileContent)
                Dim response = HttpClient.PostAsync(uploadUrl, content).Result

                If response.IsSuccessStatusCode Then
                    Dim responseText = response.Content.ReadAsStringAsync().Result
                    statusCallback?.Invoke($"SUCCESS: {remoteFileName} uploaded")
                    Return True
                Else
                    Dim errorText = response.Content.ReadAsStringAsync().Result
                    statusCallback?.Invoke($"ERROR uploading {remoteFileName}: HTTP {response.StatusCode}")
                    Return False
                End If
            End Using

        Catch ex As Exception
            statusCallback?.Invoke($"ERROR: Exception uploading {localFilePath}: {ex.Message}")
            Return False
        End Try
    End Function

    ''' <summary>
    ''' Upload multiple files from a list
    ''' </summary>
    ''' <param name="filePaths">List of file paths to upload</param>
    ''' <param name="statusCallback">Optional callback for status updates</param>
    ''' <returns>Number of files successfully uploaded</returns>
    Public Function UploadFiles(filePaths As List(Of String), Optional statusCallback As Action(Of String) = Nothing) As Integer
        Dim successCount = 0

        If filePaths Is Nothing OrElse filePaths.Count = 0 Then
            statusCallback?.Invoke("WARNING: No files to upload")
            Return 0
        End If

        statusCallback?.Invoke($"Starting upload of {filePaths.Count} file(s)...")

        For i = 0 To filePaths.Count - 1
            Dim filePath = filePaths(i)
            statusCallback?.Invoke($"[{i + 1}/{filePaths.Count}] Processing {Path.GetFileName(filePath)}...")

            If UploadFile(filePath, Nothing, statusCallback) Then
                successCount += 1
            End If
        Next

        statusCallback?.Invoke($"Upload complete: {successCount}/{filePaths.Count} files successful")
        Return successCount
    End Function

    ''' <summary>
    ''' Upload multiple files with custom remote names
    ''' </summary>
    ''' <param name="fileMapping">Dictionary of local path -> remote filename</param>
    ''' <param name="statusCallback">Optional callback for status updates</param>
    ''' <returns>Number of files successfully uploaded</returns>
    Public Function UploadFilesWithMapping(fileMapping As Dictionary(Of String, String), Optional statusCallback As Action(Of String) = Nothing) As Integer
        Dim successCount = 0

        If fileMapping Is Nothing OrElse fileMapping.Count = 0 Then
            statusCallback?.Invoke("WARNING: No files to upload")
            Return 0
        End If

        statusCallback?.Invoke($"Starting upload of {fileMapping.Count} file(s)...")

        Dim i = 1
        For Each kvp In fileMapping
            Dim localPath = kvp.Key
            Dim remoteName = kvp.Value

            statusCallback?.Invoke($"[{i}/{fileMapping.Count}] Uploading {remoteName}...")

            If UploadFile(localPath, remoteName, statusCallback) Then
                successCount += 1
            End If

            i += 1
        Next

        statusCallback?.Invoke($"Upload complete: {successCount}/{fileMapping.Count} files successful")
        Return successCount
    End Function

    ''' <summary>
    ''' Upload multiple files from a list of tuples (LocalPath, RemoteName)
    ''' </summary>
    ''' <param name="uploadList">List of tuples with (LocalPath As String, RemoteName As String)</param>
    ''' <param name="statusCallback">Optional callback for status updates</param>
    ''' <returns>Number of files successfully uploaded</returns>
    Public Function UploadFileList(uploadList As List(Of (LocalPath As String, RemoteName As String)), Optional statusCallback As Action(Of String) = Nothing) As Integer
        Dim successCount = 0

        If uploadList Is Nothing OrElse uploadList.Count = 0 Then
            statusCallback?.Invoke("WARNING: No files to upload")
            Return 0
        End If

        statusCallback?.Invoke($"Starting upload of {uploadList.Count} file(s)...")

        For i = 0 To uploadList.Count - 1
            Dim localPath = uploadList(i).LocalPath
            Dim remoteName = uploadList(i).RemoteName

            statusCallback?.Invoke($"[{i + 1}/{uploadList.Count}] Uploading {remoteName}...")

            If UploadFile(localPath, remoteName, statusCallback) Then
                successCount += 1
            End If
        Next

        statusCallback?.Invoke($"Upload complete: {successCount}/{uploadList.Count} files successful")
        Return successCount
    End Function

    ''' <summary>
    ''' Dispose resources
    ''' </summary>
    Public Sub Dispose()
        HttpClient?.Dispose()
    End Sub

End Class
