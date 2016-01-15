Unofficial memory fix for Gothic I - Gothic II: Night of Raven games changes original SmartHeap memory allocation library to more new Hoard 3.10.
New library better works on modern hardware, especially usefully on large addons, where reduces lags and OutOfMemory errors.

Install:
0) VS 2015 C++ Redistributable: https://www.microsoft.com/en-us/download/details.aspx?id=48145
1) Make backup copy shw32.dll from GothicFolder/System (don't skip this step, it's important!)
2) Copy with replace dll files from this arhive to GothicFolder/System.
On Windows XP you may use dll files from arhive WinXP_SP2_Compatibility.zip, this is the same fix code, but compiled with compatibily settings.
On x64 windows recommended used together with 4gb_patch (http://www.ntcore.com/4gb_patch.php , but this don't required).

Fix suitable for use with Gothic I - Gothic II: Night of Raven games with or without any mods.

License: GNU GPL 2.0 - free distribution and change (but if you will publish your changes, then you also will need publish your souce code under GPL 2.0)
Version: 05 (2016/01/15)
Source code: https://bitbucket.org/lviper/gothic_1_2_mem_fix/src

Best regards, lviper.
  Moscow, 2016
