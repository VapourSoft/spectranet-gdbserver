param(
  [Parameter(Mandatory=$false)][string]$ComPort = 'COM10',
  [int]$Baud = 31250,
  [int]$Port = 3333,
  [int]$ReadBuffer = 1024,
  [int]$WriteTimeoutMs = 1000,
  [switch]$Trace = $true,
  [switch]$Dtr = $false,
  [switch]$Rts = $false,
  [switch]$AutoDisableHandshakeOnTimeout = $false,
  [int]$TimeoutFailLimit = 2,
  [switch]$HoldRts = $true,
  [switch]$HoldDtr = $true,
  [ValidateSet('None','RTS','RTSXONXOFF','XONXOFF')][string]$Handshake = 'RTS'
)

function Get-Ascii([byte[]]$buf, [int]$len) {
  if ($len -le 0) { return "" }
  return -join ($buf[0..($len-1)] | ForEach-Object { if (32 -le $_ -and $_ -lt 127) { [char]$_ } else { '.' } })
}

$listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Any, $Port)
try { $listener.Server.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true) } catch {}
try {
  $listener.Start()
} catch {
  return
}

try {
  while ($true) {
    $client = $listener.AcceptTcpClient()
    Write-Host "Client connected from $($client.Client.RemoteEndPoint)"
    $client.NoDelay = $true
    $ns = $client.GetStream()

    $sp = New-Object System.IO.Ports.SerialPort $ComPort, $Baud, 'None', 8, 'One'
    $sp.ReadTimeout = 1
    $sp.WriteTimeout = 1000
    if ($Dtr) { $sp.DtrEnable = $true }
    if ($Rts) { $sp.RtsEnable = $true }
    if ($Dtr -or $HoldDtr) { $sp.DtrEnable = $true }
    if ($Rts -or $HoldRts) { $sp.RtsEnable = $true }
    switch ($Handshake) {
      'None'       { $sp.Handshake = [System.IO.Ports.Handshake]::None }
      'RTS'        { $sp.Handshake = [System.IO.Ports.Handshake]::RequestToSend }
      'RTSXONXOFF' { $sp.Handshake = [System.IO.Ports.Handshake]::RequestToSendXOnXOff }
      'XONXOFF'    { $sp.Handshake = [System.IO.Ports.Handshake]::XOnXOff }
    }
    try {
      $sp.Open()
    } catch {
      $client.Close(); continue
    }

    try {
      $rxBuf = New-Object byte[] $ReadBuffer   # TCP->SER
      $txBuf = New-Object byte[] $ReadBuffer   # SER->TCP
      while ($true) {
        try {
          if ($client.Client.Poll(0, [System.Net.Sockets.SelectMode]::SelectRead) -and $client.Client.Available -eq 0) { break }
        } catch { break }

        # TCP -> Serial
        if ($ns.DataAvailable) {
          $n = $ns.Read($rxBuf, 0, $rxBuf.Length)
          if ($n -gt 0) {
            try {
              $sp.Write($rxBuf, 0, $n)
              if ($Trace) { Write-Host "[TCP->SER] $(Get-Ascii $rxBuf $n)" }
            } catch { break }
          }
        }

        # Serial -> TCP
        $btr = $sp.BytesToRead
        if ($btr -gt 0) {
          if ($btr -gt $txBuf.Length) { $btr = $txBuf.Length }
          try { $n2 = $sp.Read($txBuf, 0, $btr) } catch { break }
          if ($n2 -gt 0 -and $client.Connected) {
            try {
              $ns.Write($txBuf, 0, $n2)
              $ns.Flush()
              if ($Trace) { Write-Host "[SER->TCP] $(Get-Ascii $txBuf $n2)" }
            } catch { break }
          }
        }

        #Start-Sleep -Milliseconds 1
      }
    } finally {
      Write-Host "Client disconnected from $($client.Client.RemoteEndPoint)"
      if ($sp -and $sp.IsOpen) { try { $sp.Close() } catch {} }
      if ($ns) { try { $ns.Dispose() } catch {} }
      if ($client) { try { $client.Close() } catch {} }
    }
  }
} finally {
  try { $listener.Stop() } catch {}
}
