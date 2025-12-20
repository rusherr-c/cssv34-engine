# CSSv34 Engine
Modified version of Source Engine 2007 with CSS v34 server-connection support

Some resources used for fixing and modifying Source Engine 2007:
* https://github.com/quiverteam/Engine
* https://github.com/uvbs/source-2007
* https://github.com/ValveSoftware/source-sdk-2013
* https://github.com/mapbase-source/source-sdk-2013
* https://github.com/Source-SDK-Archives/source-sdk-2006-ep1

## Build System:
Originally VPC(Valve Project Creator) was used but due to more better QPC(QuiverTeam Project Creator) i have decided to replace VPC to QPC

To make this thing work:
* Install Python (3.9+) https://www.python.org/downloads/
* Run _install_dependencies.bat in src folder


Compiling can be done with VS 2017, 2019, 2022, 2026 (WARNING: VS2017 not tested yet, probably needs some fixing)
## Requirements:

MFC C++ Library (can be downloaded in Visual Studio Installer)<br>
Strawberry Perl https://strawberryperl.com (if you want to compile shaders)

## Some notes
HL2 And Episodic sometimes dont show up the gameui and crash while loading the map (idk why ts shit happens)<br>
CSTRIKE needs a CSS v34 Content or CSS v92 Content (you should actually use the v92 content with v34 maps i think)

And yeah CSS v93(Last Steam Version) Content not supported, it has newer model and material versions so it will cause alot of bugs on the maps.
