# SPDX-License-Identifier: MIT
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RunDirectory,
    [Parameter(Mandatory)][ValidateSet('Windows','OpenTas','Resume','Button','Screenshot','Connected','ExportState')][string]$Action,
    [ValidateSet('Main','Sender','GBA','GC','GBATas','WiiTas')][string]$Target='Sender',
    [string]$Button='A', [ValidateRange(50,5000)][int]$Milliseconds=200,
    [string]$Name='capture', [bool]$Connected=$false
)
. "$PSScriptRoot/Common.ps1"
$run=Read-TunaRun $RunDirectory
$null=Assert-TunaProcess $run
function Resume-Tuna {
    $r=Get-TunaElement (Get-TunaWindow $run 'Main').handle
    $play=$r.FindFirst([Windows.Automation.TreeScope]::Descendants,
        [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'Play'))
    if ($play) { Invoke-TunaElement $play; Start-Sleep -Milliseconds 200 }
}
function Get-TunaGbaMenu {
    $gba=Get-TunaWindow $run 'GBA'
    [void][TunaEmulator.Native]::PostMessage([IntPtr]$gba.handle,0x7b,[IntPtr]$gba.handle,[IntPtr](-1))
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        $menus=@(Get-TunaWindows $run | Where-Object title -eq 'dolphin-emu')
        foreach($menu in $menus) {
            $r=Get-TunaElement $menu.handle
            try { $null=Find-TunaElement $r 'Connected'; return $r } catch { }
        }
        if([DateTime]::UtcNow -gt $deadline){throw 'No owned GBA context menu appeared.'}
        Start-Sleep -Milliseconds 100
    } while($true)
}
switch($Action) {
 'Windows' { Get-TunaWindows $run }
 'Resume' { Resume-Tuna }
 'OpenTas' {
    $r=Get-TunaElement (Get-TunaWindow $run 'Main').handle
    (Find-TunaElement $r 'Movie').GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
    Invoke-TunaElement (Find-TunaElement $r 'TAS Input')
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        try { $null=Get-TunaWindow $run 'GC'; $null=Get-TunaWindow $run 'GBATas'; break } catch {
            if([DateTime]::UtcNow -gt $deadline){throw}; Start-Sleep -Milliseconds 100
        }
    } while($true)
    foreach($t in @('GC','GBATas')) {
        $r=Get-TunaElement (Get-TunaWindow $run $t).handle
        $p=(Find-TunaElement $r 'Enable Controller Input').GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern)
        if($p.Current.ToggleState -eq [Windows.Automation.ToggleState]::On){$p.Toggle()}
    }
    Resume-Tuna
 }
 'Button' {
    if($Target -notin @('GC','GBATas','WiiTas')){throw 'Button requires a TAS target, not a render window.'}
    Resume-Tuna
    $r=Get-TunaElement (Get-TunaWindow $run $Target).handle
    $p=(Find-TunaElement $r $Button).GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern)
    if($p.Current.ToggleState -ne [Windows.Automation.ToggleState]::Off){throw 'TAS button is already held; refusing an ambiguous press.'}
    try { $p.Toggle(); Start-Sleep -Milliseconds $Milliseconds } finally {
        if($p.Current.ToggleState -ne [Windows.Automation.ToggleState]::Off){$p.Toggle()}
    }
    Start-Sleep -Milliseconds 100
 }
 'Screenshot' {
    Add-Type -AssemblyName System.Drawing
    $w=Get-TunaWindow $run $Target
    $rect=[TunaEmulator.Native+Rect]::new()
    if(![TunaEmulator.Native]::GetWindowRect([IntPtr]$w.handle,[ref]$rect)){throw 'Cannot read window dimensions.'}
    $width=$rect.right-$rect.left; $height=$rect.bottom-$rect.top
    if($width -le 0 -or $height -le 0 -or $width -gt 8192 -or $height -gt 8192){throw 'Invalid/minimized window dimensions.'}
    $output=Get-TunaEvidencePath $RunDirectory $Name '.png'
    $bitmap=[Drawing.Bitmap]::new($width,$height); $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $dc=$graphics.GetHdc()
        try { if(![TunaEmulator.Native]::PrintWindow([IntPtr]$w.handle,$dc,2)){throw 'PrintWindow failed.'} } finally { $graphics.ReleaseHdc($dc) }
        $bitmap.Save($output,[Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
    $output
 }
 'Connected' {
    $menu=Get-TunaGbaMenu
    $element=Find-TunaElement $menu 'Connected'
    $p=$element.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern)
    $on=$p.Current.ToggleState -eq [Windows.Automation.ToggleState]::On
    if($on -ne $Connected){Invoke-TunaElement $element}
    else {
        # Dismiss only this owned popup, using WM_CLOSE (no global Escape).
        foreach($w in @(Get-TunaWindows $run | Where-Object title -eq 'dolphin-emu')) {
            [void][TunaEmulator.Native]::PostMessage([IntPtr]$w.handle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        }
    }
 }
 'ExportState' {
    $output=Get-TunaEvidencePath $RunDirectory $Name '.ss0'
    $menu=Get-TunaGbaMenu
    (Find-TunaElement $menu 'Save State').GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
    try { Invoke-TunaElement (Find-TunaElement $menu 'Export State...') } catch {
        # Qt Invoke may time out while its modal native Save dialog is open.
        if(!@(Get-TunaWindows $run | Where-Object title -eq 'Select a File').Count){throw}
    }
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {
        $dialogs=@(Get-TunaWindows $run | Where-Object title -eq 'Select a File')
        if($dialogs.Count -eq 1){break}
        if([DateTime]::UtcNow -gt $deadline){throw 'Expected one owned state-export Save dialog.'}
        Start-Sleep -Milliseconds 100
    } while($true)
    # Avoid recursive UIAutomation enumeration of native Explorer dialogs.
    $children=[TunaEmulator.Native]::Children($dialogs[0].handle)
    $edit=@($children | Where-Object {$_.kind -eq 'Edit' -and $_.id -eq 1001})
    $save=@($children | Where-Object {$_.kind -eq 'Button' -and $_.id -eq 1})
    if($edit.Count -ne 1 -or $save.Count -ne 1){throw 'Unrecognized Save dialog layout; do not guess handles.'}
    $result=[IntPtr]::Zero
    if([TunaEmulator.Native]::SetText([IntPtr]$edit[0].handle,0xc,[IntPtr]::Zero,$output,2,2000,[ref]$result) -eq [IntPtr]::Zero){throw 'Could not set state export filename.'}
    [void][TunaEmulator.Native]::PostMessage([IntPtr]$dialogs[0].handle,0x111,[IntPtr]1,[IntPtr]$save[0].handle)
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    while(!(Test-Path -LiteralPath $output)) {
        if([DateTime]::UtcNow -gt $deadline){throw 'State export did not create the output file.'}
        Start-Sleep -Milliseconds 100
    }
    $output
 }
}
Write-TunaAction $RunDirectory $Action @{target=$Target;button=$Button;milliseconds=$Milliseconds;name=$Name;connected=$Connected}
