# SPDX-License-Identifier: MIT
# WASAPI loopback captures the default output endpoint, not only one application.
# Keep other applications silent. No microphone capture or system volume changes.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$RunDirectory,[string]$Name='playback',
 [ValidateRange(1,600)][int]$Seconds=6,[string]$NAudio,
 [switch]$NoPress
)
. "$PSScriptRoot/Common.ps1"
$run=Read-TunaRun $RunDirectory;$null=Assert-TunaProcess $run
if(!$NAudio){$NAudio=Join-Path (Get-TunaRoot) '.local/tools/naudio-1.10.0/package/lib/net35/NAudio.dll'}
$NAudio=(Resolve-Path -LiteralPath $NAudio).Path
$null=[Reflection.Assembly]::LoadFrom($NAudio)
if(-not ('TunaEmulator.Loopback' -as [type])) {
 Add-Type -ReferencedAssemblies $NAudio -TypeDefinition @'
using System;
using System.Threading;
using NAudio.Wave;
namespace TunaEmulator {
 public sealed class Loopback : IDisposable {
  WasapiLoopbackCapture capture;
  WaveFileWriter writer;
  ManualResetEvent stopped=new ManualResetEvent(false);
  readonly object sync=new object();
  Exception error;
  public string Format { get { return capture.WaveFormat.ToString(); } }
  public long Bytes;
  public Loopback(string path) {
   capture=new WasapiLoopbackCapture();writer=new WaveFileWriter(path,capture.WaveFormat);
   capture.DataAvailable+=(s,e)=>{lock(sync){writer.Write(e.Buffer,0,e.BytesRecorded);Bytes+=e.BytesRecorded;}};
   capture.RecordingStopped+=(s,e)=>{error=e.Exception;stopped.Set();};
  }
  public void Start(){capture.StartRecording();}
  public void Dispose(){
   capture.StopRecording();
   bool ended=stopped.WaitOne(5000);
   lock(sync){writer.Dispose();}
   capture.Dispose();stopped.Dispose();
   if(!ended) throw new Exception("Loopback did not stop within its deadline.");
   if(error!=null) throw error;
  }
 }
}
'@
}
$output=Get-TunaEvidencePath $RunDirectory $Name '.wav'
$capture=[TunaEmulator.Loopback]::new($output)
$format=$capture.Format
try {
 $capture.Start();Start-Sleep -Milliseconds 500
 if(!$NoPress){$null=& "$PSScriptRoot/Control-Dolphin.ps1" -RunDirectory $RunDirectory -Action Button -Target GBATas -Button A}
 Start-Sleep -Seconds $Seconds
} finally {$capture.Dispose()}
if($capture.Bytes -eq 0){throw 'No loopback audio frames were captured.'}
Write-TunaAction $RunDirectory 'audio-loopback' @{file=$output;format=$format;bytes=$capture.Bytes;naudioSha256=(Get-FileHash $NAudio).Hash;
 scope='default output endpoint; other applications must be silent';volumeChanged=$false;microphone=$false}
$output
