Imports System.Net.Http
Imports System.Net.Http.Headers
Imports System.IO

''' <summary>
''' ESP32 MUD File Upload Utility
''' Provides methods to upload files to the ESP32 MUD device via HTTP
''' </summary>
Public Class ESP32FileUploader

    Private ReadOnly DeviceIP As String
    Private ReadOnly Port As Integer = 8080
    Private ReadOnly UploadUrl As String

    ''' <summary>
    ''' Initialize with device IP address
    ''' </summary>
    Public Sub New(deviceIP As String, Optional port As Integer = 8080)
        Me.DeviceIP = deviceIP
        Me.Port = port
        Me.UploadUrl = $"http://{deviceIP}:{port}/upload"
    End Sub

    ''' <summary>
    ''' Upload a single file to the device
    ''' </summary>
    ''' <param name="filePath">Full path to the file to upload</param>
    ''' <returns>True if successful, False otherwise</returns>
    Public Function UploadFile(filePath As String) As Boolean
        Return UploadFile(filePath, Nothing)
    End Function

    ''' <summary>
    ''' Upload a single file with progress callback
    ''' </summary>
    ''' <param name="filePath">Full path to the file to upload</param>
    ''' <param name="progressCallback">Optional callback for progress updates</param>
    ''' <returns>True if successful, False otherwise</returns>
    Public Function UploadFile(filePath As String, progressCallback As Action(Of String)) As Boolean
        Try
            If Not File.Exists(filePath) Then
                progressCallback?.Invoke($"ERROR: File not found: {filePath}")
                Return False
            End If

            Dim fileName = Path.GetFileName(filePath)
            Dim fileSize = New FileInfo(filePath).Length
            progressCallback?.Invoke($"Uploading {fileName} ({fileSize} bytes)...")

            Using client As New HttpClient()
                client.Timeout = TimeSpan.FromSeconds(60)  ' Increased timeout

                ' Create multipart form data
                Using content As New MultipartFormDataContent()
                    ' Add file to form
                    Using fileStream = File.OpenRead(filePath)
                        Dim fileContent As New StreamContent(fileStream)
                        fileContent.Headers.ContentDisposition = New ContentDispositionHeaderValue("form-data") With {
                            .Name = """file""",
                            .FileName = $"""{fileName}"""
                        }
                        fileContent.Headers.ContentType = New MediaTypeHeaderValue("application/octet-stream")
                        content.Add(fileContent, "file")

                        Try
                            ' Send request
                            Dim response = client.PostAsync(UploadUrl, content).Result
                            
                            If response.IsSuccessStatusCode Then
                                Dim responseContent = response.Content.ReadAsStringAsync().Result
                                progressCallback?.Invoke($"✓ {fileName} uploaded successfully")
                                Return True
                            Else
                                progressCallback?.Invoke($"✗ {fileName} failed: HTTP {response.StatusCode}")
                                progressCallback?.Invoke($"  Response: {response.Content.ReadAsStringAsync().Result}")
                                Return False
                            End If
                        Catch webEx As HttpRequestException
                            ' Extract the real error from nested exceptions
                            Dim actualError = If(webEx.InnerException?.Message, webEx.Message)
                            progressCallback?.Invoke($"✗ {fileName} failed: Connection error")
                            progressCallback?.Invoke($"  {actualError}")
                            Return False
                        Catch aggEx As AggregateException
                            ' Handle AggregateException properly
                            Dim actualError = If(aggEx.InnerException?.Message, aggEx.Message)
                            progressCallback?.Invoke($"✗ {fileName} failed: {actualError}")
                            Return False
                        End Try
                    End Using
                End Using
            End Using
        Catch ex As Exception
            ' Get the deepest error message
            Dim deepError = ExtractDeepestError(ex)
            progressCallback?.Invoke($"ERROR: {deepError}")
            Return False
        End Try
    End Function

    ''' <summary>
    ''' Extract the most meaningful error message from nested exceptions
    ''' </summary>
    Private Function ExtractDeepestError(ex As Exception) As String
        If ex Is Nothing Then Return "Unknown error"
        
        If ex.InnerException IsNot Nothing Then
            Return ExtractDeepestError(ex.InnerException)
        End If
        
        Return ex.Message
    End Function

    ''' <summary>
    ''' Upload multiple files from a directory
    ''' </summary>
    ''' <param name="folderPath">Path to folder containing files to upload</param>
    ''' <param name="progressCallback">Optional callback for progress updates</param>
    ''' <returns>Number of successfully uploaded files</returns>
    Public Function UploadFolder(folderPath As String, Optional progressCallback As Action(Of String) = Nothing) As Integer
        Try
            If Not Directory.Exists(folderPath) Then
                progressCallback?.Invoke($"ERROR: Folder not found: {folderPath}")
                Return 0
            End If

            Dim files = Directory.GetFiles(folderPath)
            Dim successCount = 0

            progressCallback?.Invoke($"Found {files.Length} file(s) to upload")
            progressCallback?.Invoke("")

            For Each filePath In files
                If UploadFile(filePath, progressCallback) Then
                    successCount += 1
                End If
                ' Small delay between uploads
                Threading.Thread.Sleep(500)
            Next

            progressCallback?.Invoke("")
            progressCallback?.Invoke($"Upload complete: {successCount}/{files.Length} files successful")
            Return successCount
        Catch ex As Exception
            progressCallback?.Invoke($"ERROR: {ex.Message}")
            Return 0
        End Try
    End Function

    ''' <summary>
    ''' Test connection to the device
    ''' </summary>
    ''' <returns>True if device is reachable, False otherwise</returns>
    Public Function TestConnection() As Boolean
        Try
            Using client As New HttpClient()
                client.Timeout = TimeSpan.FromSeconds(10)
                Dim response = client.GetAsync(UploadUrl).Result
                Return response.IsSuccessStatusCode OrElse response.StatusCode = System.Net.HttpStatusCode.MethodNotAllowed
            End Using
        Catch ex As Exception
            ' Connection failed
            Return False
        End Try
    End Function

    ''' <summary>
    ''' Detailed diagnostic test - returns diagnostic info instead of boolean
    ''' </summary>
    Public Function DiagnoseConnection(progressCallback As Action(Of String)) As Boolean
        progressCallback?.Invoke($"Diagnosing connection to {DeviceIP}:{Port}...")
        progressCallback?.Invoke("")

        ' Test 1: Ping (DNS resolution)
        progressCallback?.Invoke("Test 1: DNS Resolution...")
        Try
            Dim host = System.Net.Dns.GetHostEntry(DeviceIP)
            progressCallback?.Invoke($"  ✓ DNS resolved to: {host.HostName}")
        Catch ex As Exception
            progressCallback?.Invoke($"  ✗ DNS failed: {ex.Message}")
            Return False
        End Try

        ' Test 2: TCP Connection
        progressCallback?.Invoke("Test 2: TCP Connection...")
        Try
            Using socket As New System.Net.Sockets.Socket(System.Net.Sockets.AddressFamily.InterNetwork, 
                                                          System.Net.Sockets.SocketType.Stream, 
                                                          System.Net.Sockets.ProtocolType.Tcp)
                socket.ReceiveTimeout = 5000
                socket.SendTimeout = 5000
                socket.Connect(DeviceIP, Port)
                progressCallback?.Invoke($"  ✓ TCP connection successful to {DeviceIP}:{Port}")
                socket.Close()
            End Using
        Catch ex As Exception
            progressCallback?.Invoke($"  ✗ TCP connection failed: {ex.Message}")
            Return False
        End Try

        ' Test 3: HTTP Request
        progressCallback?.Invoke("Test 3: HTTP GET Request...")
        Try
            Using client As New HttpClient()
                client.Timeout = TimeSpan.FromSeconds(10)
                Dim response = client.GetAsync(UploadUrl).Result
                progressCallback?.Invoke($"  ✓ HTTP response: {response.StatusCode}")
                
                If response.IsSuccessStatusCode Then
                    Dim content = response.Content.ReadAsStringAsync().Result
                    progressCallback?.Invoke($"  Content preview: {content.Substring(0, Math.Min(100, content.Length))}")
                End If
            End Using
        Catch ex As Exception
            Dim deepError = ExtractDeepestError(ex)
            progressCallback?.Invoke($"  ✗ HTTP request failed: {deepError}")
            Return False
        End Try

        progressCallback?.Invoke("")
        progressCallback?.Invoke("✓ All diagnostic tests passed!")
        Return True
    End Function

End Class