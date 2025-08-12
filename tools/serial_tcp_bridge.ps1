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
  [ValidateSet('None','RTS','RTSXONXOFF','XONXOFF')][string]$Handshake = 'None'
)

Write-Host "[bridge] Preparing TCP listener :$Port <-> $ComPort @ ${Baud}bps (8N1) trace=$($Trace.IsPresent)"

function New-HexDump([byte[]]$buf, [int]$len) {
  if ($len -le 0) { return "len=0" }
  $hex = -join ($buf[0..($len-1)] | ForEach-Object { $_.ToString('x2') })
  $ascii = -join ($buf[0..($len-1)] | ForEach-Object { if (32 -le $_ -and $_ -lt 127) { [char]$_ } else { '.' } })
  return "len=$len hex=$hex ascii='$ascii'"
}

$listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Loopback, $Port)
try { $listener.Server.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true) } catch {}
try {
  $listener.Start()
  Write-Host "[bridge] Listening on TCP :$Port"
} catch {
  Write-Error "[bridge] Failed to start listener on :$Port - $($_.Exception.Message)"
  return
}

try {
  while ($true) {
    Write-Host "[bridge] Waiting for TCP client..."
    $client = $listener.AcceptTcpClient()
    $client.NoDelay = $true
    $ns = $client.GetStream()
    Write-Host "[bridge] TCP client connected from $($client.Client.RemoteEndPoint)"

    # (Re)open serial each session for a clean state
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
      Write-Host "[bridge] Serial opened $ComPort"
    } catch {
      Write-Error "[bridge] Failed to open $ComPort : $($_.Exception.Message)"
      $client.Close(); continue
    }

    try {
      $rxBuf = New-Object byte[] $ReadBuffer   # TCP->SER
      $txBuf = New-Object byte[] $ReadBuffer   # SER->TCP
      $serBytesOut = 0
      $serBytesIn = 0
      $tcpLoops = 0
      $lastStatus = Get-Date
      $lastCtlStatus = Get-Date
      while ($true) {
        # Detect TCP disconnect
        try {
          if ($client.Client.Poll(0, [System.Net.Sockets.SelectMode]::SelectRead) -and $client.Client.Available -eq 0) { Write-Host "[bridge] TCP client disconnected"; break }
        } catch { Write-Host "[bridge] TCP poll error: $($_.Exception.Message)"; break }

        # TCP -> Serial
        if ($ns.DataAvailable) {
          $n = $ns.Read($rxBuf, 0, $rxBuf.Length)
          if ($n -gt 0) {
            try {
              $sp.Write($rxBuf, 0, $n)
              $serBytesOut += $n
              if ($Trace) { Write-Host "[bridge] TCP->SER $(New-HexDump $rxBuf $n)" }
            } catch { Write-Warning "[bridge] Serial write error: $($_.Exception.Message)"; break }
          }
        }

        # Serial -> TCP
        $btr = $sp.BytesToRead
        if ($btr -gt 0) {
          if ($btr -gt $txBuf.Length) { $btr = $txBuf.Length }
          try { $n2 = $sp.Read($txBuf, 0, $btr) } catch { Write-Warning "[bridge] Serial read error: $($_.Exception.Message)"; break }
          if ($n2 -gt 0 -and $client.Connected) {
            try {
              $ns.Write($txBuf, 0, $n2)
              $ns.Flush()
              $serBytesIn += $n2
              if ($Trace) { Write-Host "[bridge] SER->TCP $(New-HexDump $txBuf $n2)" }
            } catch { Write-Warning "[bridge] TCP write error: $($_.Exception.Message)"; break }
          }
        }

        Start-Sleep -Milliseconds 1
        $tcpLoops++
        $now = Get-Date
        if ($HoldRts -and $Handshake -eq 'None' -and -not $sp.RtsEnable) { try { $sp.RtsEnable = $true } catch {} }
        if ($HoldDtr -and $Handshake -eq 'None' -and -not $sp.DtrEnable) { try { $sp.DtrEnable = $true } catch {} }
        if ($now -gt $lastStatus.AddMilliseconds(500)) {
          if ($Trace) {
            Write-Host "[bridge] status bytes_out=$serBytesOut bytes_in=$serBytesIn loops=$tcpLoops ser_in_wait=$($sp.BytesToRead) connected=$($client.Connected)" }
          $lastStatus = $now
        }
        if ($Trace -and $now -gt $lastCtlStatus.AddSeconds(2)) {
          try {
            $cts = $sp.CtsHolding; $dsr=$sp.DsrHolding; $cd=$sp.CDHolding; $rts=$sp.RtsEnable; $dtr=$sp.DtrEnable
            Write-Host "[bridge] ctrl CTS=$cts DSR=$dsr CD=$cd RTS=$rts DTR=$dtr handshake=$Handshake"
          } catch {}
          $lastCtlStatus = $now
        }
      }
    } catch {
      Write-Warning "[bridge] Loop error: $($_.Exception.Message)"
    } finally {
      if ($sp -and $sp.IsOpen) { try { $sp.Close() } catch {} }
      if ($ns) { try { $ns.Dispose() } catch {} }
      if ($client) { try { $client.Close() } catch {} }
      Write-Host "[bridge] Disconnected."
    }
  }
} finally {
  try { $listener.Stop() } catch {}
  Write-Host "[bridge] Listener stopped."
}
