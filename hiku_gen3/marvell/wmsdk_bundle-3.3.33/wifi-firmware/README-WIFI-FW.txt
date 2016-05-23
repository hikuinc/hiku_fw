This readme explains the naming conventions used for various
Wi-Fi Firmware binaries in this directory.
=========================================================

There are 2 types of firmware binaries for each Wi-fi chip.

1. Manufacturing firmware.
2. Production firmware.

Naming convention used :

Production firmware:
---------------------
sd8xxx_uapsta.bin.xz

Where,
1. sd      : SDIO interface Wi-Fi card.
2. 8xxx    : Chip name.
	     e.g. : 8787, 8782, 8777, 8801, mw30x etc.
3. uapsta  : uAP and station support.
4. bin     : Binary file.
5. xz      : XZ compressed binary image.
             If not present it is an uncompressed
	     image.

Manufacturing firmware:
---------------------
sd8xxx_mfg.bin

Where,
1. sd      : SDIO interface Wi-Fi card.
2. 8xxx    : Chip name.
	     e.g. : 8787, 8782, 8777, 8801, mw30x etc.
3. mfg     : Manufacturing firmware.
4. bin     : Binary file.

Board Wi-Fi firmware Pairing
============================

------------------------------------------------------------------
|  Board       |   Wi-Fi Chip    |  Firmware 		 	 |
------------------------------------------------------------------
|  aw-cu282    |   8782          |  sd8782_uapsta.bin.xz	 |
-----------------------------------------------------------------
|  aw-cu288    |   8801          |  sd8801_uapsta.bin.xz	 |
------------------------------------------------------------------
|  aw-cu277    |   8777          |  sd8777_uapsta.bin.xz	 |
-----------------------------------------------------------------
|  aw-cu300    |   mw30x         |  mw30x_uapsta.bin.xz		 |
------------------------------------------------------------------
|  lk20-v3     |   8787	         |  sd8787_uapsta.bin.xz	 |
------------------------------------------------------------------
|  mc200_8801  |   8801          |  sd8801_uapsta.bin.xz         |
------------------------------------------------------------------
|  mw300_rd    |   mw30x	 |  mw30x_uapsta.bin.xz	 	 |
------------------------------------------------------------------

Creating Compressed Wi-Fi Firmware binary
=========================================

Note: .xz firmware images are the pre-built compressed firmware images of existing wireless firmwares.
Compressed firmware images for new releases can be created using following command.

$./tools/bin/Windows/xz --check=crc32 --lzma2=dict=16KiB /path/to/uncompressed-wlan-firmware
