# SPDX-License-Identifier: MIT
# Decode audio-related GBA state without modifying the emulator.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$State,[string]$Symbols)
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $State).Path)
if($bytes.Length -ne 0x61000 -or [BitConverter]::ToUInt32($bytes,0) -ne 0x01000009){throw 'Unsupported state: require raw mGBA v9 (397312 bytes).'}
function U16([int]$io) { [BitConverter]::ToUInt16($bytes,0x400+$io) }
function U32([int]$io) { [BitConverter]::ToUInt32($bytes,0x400+$io) }
function Hex([int]$value,[int]$digits) { '0x' + $value.ToString('x' + $digits) }
$soundcntH=U16 0x82; $soundcntX=U16 0x84; $soundbias=U16 0x88; $soundcntL=U16 0x80
$result=[ordered]@{
 utc=[DateTime]::UtcNow.ToString('o'); state=(Resolve-Path -LiteralPath $State).Path
 soundcnt_l=(Hex $soundcntL 4); soundcnt_h=(Hex $soundcntH 4); soundcnt_x=(Hex $soundcntX 4); soundbias=(Hex $soundbias 4)
 master_enable=[bool]($soundcntX -band 0x80)
 dsound_a_volume_percent=$(if($soundcntH -band 0x04){100}else{50})
 dsound_b_volume_percent=$(if($soundcntH -band 0x08){100}else{50})
 dsound_a_left=[bool]($soundcntH -band 0x0200); dsound_a_right=[bool]($soundcntH -band 0x0100)
 dsound_a_uses_timer0=(-not ($soundcntH -band 0x0400))
 psg_volume_percent=$(switch($soundcntH -band 3){0{25}1{50}2{100}default{100}})
 dmg_master_volume_l=($soundcntL -band 0x07); dmg_master_volume_r=(($soundcntL -shr 4) -band 0x07)
 timer0_reload=(Hex (U16 0x100) 4); timer0_control=(Hex (U16 0x102) 4)
 timer1_reload=(Hex (U16 0x104) 4); timer1_control=(Hex (U16 0x106) 4)
 dma1_source=(Hex (U32 0xBC) 8); dma1_destination=(Hex (U32 0xC0) 8); dma1_control=(Hex (U32 0xC4) 8)
 dma1_enabled=[bool]((((U32 0xC4) -shr 30) -band 1))
 dma1_word_count=((U32 0xC4) -band 0xFFFF)
 scope='register snapshot only; does not prove audible output'
}
if($Symbols) {
    $lookup=@{}
    foreach($line in Get-Content -LiteralPath (Resolve-Path -LiteralPath $Symbols).Path){
        if($line -match '^([0-9a-fA-F]{8})\s+\S\s+(\S+)$'){ $lookup[$Matches[2]]=[Convert]::ToUInt32($Matches[1],16) }
    }
    $counters=[ordered]@{}
    foreach($name in @('audio_played','audio_underruns','audio_active')){
        if(!$lookup.ContainsKey($name)){continue}
        $addr=$lookup[$name]
        if($addr -ge 0x03000000 -and $addr -lt 0x03008000){ $counters[$name]=[BitConverter]::ToUInt32($bytes,0x19000+($addr-0x03000000)) }
    }
    $result['iwramCounters']=$counters
}
[pscustomobject]$result
