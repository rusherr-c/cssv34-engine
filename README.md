# CSSv34 Engine

Source Engine 2007 but with CSSv34 server-connection support.

Sources:
* https://github.com/uvbs/source-2007 .................[Source Engine 2007, 2012]
* https://github.com/nillerusr/source-engine ......[TF2, April 2020]
* https://github.com/EpicSentry/HL2-CSGO ........[CSGO, April 2020]

Features:<br>
* Recompiled VPC, src from TF2 leak
* Updated headers from CSGO and TF2 leak
* VPK support
* Removed Scaleform.
* Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc)
* Some VGUI stuff ported from TF2 leak.
* VPhysics, havok\ivp code included.
* CVAR culling disabled.
* DirectX SDK from Summer 2004 (doesn't require d3dx9_**.dll)
* standard shaders from Source SDK 2006 (partially)
* vaudio_minimp3 from TF2 leak
* Deleted bink video

# Troubleshooting compiling
Having problems building the project? Make sure you have the following:<br>
- Windows 10 SDK: https://developer.microsoft.com/en-us/windows/downloads<br>
- MSVC 145 or 143 (Build tools): Available under the "Individual Components" section of the Visual Studio Installer.<br>
VS 2022-2026 should work out of the box with no additional changes necessary.<br>

