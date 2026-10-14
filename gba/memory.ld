/* Augments devkitARM's gba_mb.ld. The track is never part of the boot image. */
SECTIONS {
    .track 0x02010000 (NOLOAD) : {
        __track_start = .;
        KEEP(*(.track))
        __track_end = .;
    }
}
ASSERT(__ewram_overlay_end <= 0x02010000, "Resident image exceeds 64 KiB budget")
ASSERT(SIZEOF(.track) == 196608, "Track reservation must be 192 KiB")
ASSERT(__track_end <= 0x02040000, "Track exceeds EWRAM")
ASSERT(__data_end__ <= 0x03007000, "IWRAM lacks 3 KiB stack/BIOS headroom")
