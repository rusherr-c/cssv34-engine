## CSSv34 Engine

This project is aimed to improve the leaked Source 2007 code, to make it more stable and to make it compatible with v34 servers.

#### Sources:
* [quiver-engine](https://github.com/quiverteam/engine)
* [source-sdk-2006](https://github.com/Source-SDK-Archives/source-sdk-2006-ep1)
* [my cssv34-sdk](https://github.com/rusherr-c/cssv34-sdk)
* [source-2007](https://github.com/uvbs/source-2007)
* [nillerusr's source-engine](https://github.com/nillerusr/source-engine)
* [csgo engine](https://github.com/EpicSentry/HL2-CSGO)

---

#### Features:
* Recompiled VPC, src from TF2 leak
* Updated headers from CSGO and TF2 leak
* PreInstalled RevEmu 9.85
* VPK support
* Filesystem from TF2 leak (less hardcoded stuff, allows for 'custom' folder, etc)
* Some VGUI stuff ported from TF2 leak.
* VPhysics, havok\ivp code included.
* CVAR culling disabled.
* DirectX SDK from Summer 2004 (doesn't require d3dx9_**.dll)
* vaudio_minimp3 from TF2 leak
* Deleted bink video
* Rewrited serverbrowser (no longer depends on steam)
* Support for gamemonitoring.net server list in serverbrowser
* Some additional engine fixes from TF2 build

### Currently known problems:
* sv_pure is broken
* some netmessages is fully incompatible with v34 version
* most of datatables is incompatible with v34 and requires fixes

### Problems with creating solutions
If you're somehow having problems with `MkSln**.bat` scripts, run `src\VCReg_Fix.bat`.

---

## License:
[PROJECT TERMS AND LICENSE](https://github.com/rusherr-c/cssv34-engine/blob/dev/LICENSE)<br>
[THIRD PARTY LEGAL NOTICES](https://github.com/rusherr-c/cssv34-engine/blob/dev/thirdpartylegalnotices.txt)

## Contributing:
Please read [CONTRIBUTING.md](https://github.com/rusherr-c/cssv34-engine/blob/dev/CONTRIBUTING.md)

---

## Troubleshooting compiling
Having issues building the project? Make sure you have read the [CONTRIBUTING.md](https://github.com/rusherr-c/cssv34-engine/blob/dev/CONTRIBUTING.md)

## Using CSS v34 Content
You need to use original CSS v34 content:
- run `create_game_junctions.bat` and type paths to `<CSSv34 Content Path>\hl2` and `<CSSv34 Content Path>\cstrike` folders
