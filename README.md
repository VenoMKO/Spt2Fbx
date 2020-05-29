## Spt2Fbx

A simple tool that generates static meshes from .spt files and saves them as .fbx.
The release version supports speed trees up to version 4.1

Since the output is a static mesh all wind animations are discarded. 
You will have to create a billboard material/shader for leafs to replicate SpeedTree's leafs behaviour.

## Usage

Drag & drop a folder or an .spt on the Spt2Fbx.exe.
The program will save .fbx files next to .spt sources.
Tested on WinXP SP2 and Win10.
***Make sure SpeedTreeRT.dll is in the same folder with the Spt2Fbx.exe***

Download: [GitHub - Spt2Fbx.zip](https://github.com/VenoMKO/Spt2Fbx/releases/download/v1.0/Spt2Fbx.zip)

## Building

You will need third party libs:
  - FbxSdk
  - dirent
  - SpeedTreeRT SDK 4.1

Depending on the version of SpeedTree you will need Visual Studio 2005 or 2008
