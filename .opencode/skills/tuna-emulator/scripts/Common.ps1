# SPDX-License-Identifier: MIT
$ErrorActionPreference = 'Stop'
if (-not ('TunaEmulator.Native' -as [type])) {
Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
namespace TunaEmulator {
 public class Window { public long handle; public string title; public int id; public string kind; }
 public static class Native {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h, EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SendMessageTimeoutW")]
  public static extern IntPtr SetText(IntPtr h,uint m,IntPtr w,string text,uint flags,uint timeout,out IntPtr result);
  public struct Rect { public int left,top,right,bottom; }
  static Window Describe(IntPtr h) {
   var s=new StringBuilder(1024);var c=new StringBuilder(256);
   GetWindowText(h,s,s.Capacity);GetClassName(h,c,c.Capacity);
   return new Window {handle=h.ToInt64(),title=s.ToString(),id=GetDlgCtrlID(h),kind=c.ToString()};
  }
  public static Window[] Windows(int pid) {
   var result=new List<Window>();
   EnumWindows((h,p)=>{uint owner;GetWindowThreadProcessId(h,out owner);
    if(owner==pid && IsWindowVisible(h)) result.Add(Describe(h));return true;},IntPtr.Zero);
   return result.ToArray();
  }
  public static Window[] Children(long parent) {
   var result=new List<Window>();EnumChildWindows(new IntPtr(parent),(h,p)=>{result.Add(Describe(h));return true;},IntPtr.Zero);
   return result.ToArray();
  }
 }
}
'@
}
[void][TunaEmulator.Native]::SetProcessDPIAware()
function Get-TunaRoot { (Resolve-Path "$PSScriptRoot/../../../..").Path }
function Read-TunaRun([string]$Directory) {
    $path = Join-Path $Directory 'manifest.json'
    if (!(Test-Path -LiteralPath $path)) { throw "Missing run manifest: $path" }
    Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}
function Assert-TunaProcess($Run) {
    $p = Get-Process -Id $Run.processId -ErrorAction SilentlyContinue
    if (!$p) { throw 'Owned emulator has exited.' }
    if ($p.ProcessName -ne 'Dolphin' -or $p.Path -ne $Run.dolphin -or $p.StartTime.ToUniversalTime().ToString('o') -ne $Run.processStartUtc) {
        throw 'PID/executable/start time mismatch: refusing to control an unrelated process.'
    }
    $p
}
function Get-TunaWindows($Run) {
    $null = Assert-TunaProcess $Run
    [TunaEmulator.Native]::Windows([int]$Run.processId)
}
function Get-TunaWindow($Run,[string]$Target) {
    $windows = @(Get-TunaWindows $Run)
    $matches = @($windows | Where-Object {
        $window = $_
        switch ($Target) {
            'Main' { $window.title -match '^Dolphin [^|]+$' }
            'Sender' { $window.title -match '^Dolphin .*\|' }
            'GBA' { $window.title -match '^GBA2 \|' }
            'GC' { $window.title -eq 'GameCube TAS Input 1' }
            'GBATas' { $window.title -eq 'GBA TAS Input 2' }
            'WiiTas' { $window.title -like 'Wii TAS Input 1*' }
        }
    })
    if ($matches.Count -ne 1) { throw "Expected one owned $Target window; found $($matches.Count). Use Windows to inspect." }
    $matches[0]
}
function Get-TunaElement([long]$Handle) {
    Add-Type -AssemblyName UIAutomationClient
    Add-Type -AssemblyName UIAutomationTypes
    [Windows.Automation.AutomationElement]::FromHandle([IntPtr]$Handle)
}
function Find-TunaElement($Parent,[string]$Name) {
    $element = $Parent.FindFirst([Windows.Automation.TreeScope]::Descendants,
        [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$Name))
    if (!$element) { throw "No owned UI control named '$Name'." }
    $element
}
function Invoke-TunaElement($Element) {
    $Element.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
}
function Write-TunaAction([string]$Directory,[string]$Action,$Details) {
    @{utc=[DateTime]::UtcNow.ToString('o');action=$Action;details=$Details} |
        ConvertTo-Json -Depth 6 -Compress | Add-Content -LiteralPath (Join-Path $Directory 'actions.jsonl')
}
function Get-TunaEvidencePath([string]$Directory,[string]$Name,[string]$Extension) {
    if ($Name -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Evidence name must contain only letters, digits, underscore or hyphen.' }
    $path = Join-Path $Directory ($Name + $Extension)
    if (Test-Path -LiteralPath $path) { throw "Evidence already exists: $path. Use a new name." }
    $path
}
