# CSPSP

![screenshot1](https://user-images.githubusercontent.com/2881968/173958305-68c42c56-7d0d-49db-8a21-fbd25b0f7e6c.png)
![screenshot2](https://user-images.githubusercontent.com/2881968/173957027-63d38e88-00fd-4d98-91b7-109fe53518d4.png)

CSPSP is a homebrew game for the Sony PSP, created in 2006 and updated through 2011. It is a 2d top-down shooter heavily based on Counter-Strike, with similar round-based team gameplay and weapon selection. It features fully multiplayer gameplay via community-hosted servers.

The game is unfortunately mostly dead now, but for those who are nostalgic, the game is still available to download - see the [Releases](https://github.com/kevinbchen/cspsp/releases) page.

- Website / master server: https://cspsp.appspot.com
- CSPSP Server application: https://github.com/kevinbchen/cspspserver

> **Warning**: I created this project back in high school and didn't expect to open source it, so the code is messy and, to put it mildy, not well written. I would not use this as any kind of reference :)

## Playing

CSPSP should run on any PSP with a custom firmware; simply copy the game into ms0:/PSP/GAME/ like any other homebrew. The most recent v1.92b release has some very minor fixes to get multiplayer working again, though there probably won't be any other players or servers online.

![controls](https://user-images.githubusercontent.com/2881968/173964080-ab480a79-af94-454a-bdfe-a2a56b491cad.png)

### PPSSPP
You can also run CSPSP using the [PPSSPP](https://www.ppsspp.org/) emulator, though the latest official release does not yet support online functionality. However, online/infrastructure support is in development (https://github.com/hrydgard/ppsspp/issues/14256), and you can find links to test builds that do already support CSPSP online (e.g. https://github.com/hrydgard/ppsspp/issues/14256#issuecomment-1136256118).

### Android

This fork includes a native Android port built on the JGE engine. It renders with OpenGL ES 1.1, plays audio via SoundPool/MediaPlayer, and uses POSIX sockets for networking — so it connects to the same master server and community-hosted game servers as the PSP and PPSSPP versions.

**Controls**: PS4 (DualShock 4) controller via Bluetooth or USB OTG.
- Left stick: movement
- D-pad / face buttons: navigate menus, buy menu, actions
- L1/R1: aim/secondary action
- Start/Select: menu actions

Touch overlay controls are planned for a future release.

**Building the Android APK** (requires Android Studio + NDK):
```
git clone https://github.com/SarahRoseLives/csandroid.git
cd csandroid/android
gradlew assembleDebug
```
The APK is at `android/app/build/outputs/apk/debug/app-debug.apk`.

## Building (PSP)

The repo has been updated and tested to build with Minimalist PSPSDK 0.10.0 on Windows.
1. Download and install [MINPSPW 0.10.0](https://sourceforge.net/projects/minpspw/files/SDK%20%2B%20devpak/pspsdk%200.10.0/)
2. Clone the repo and go to `jge/Projects/cspsp`.
3. `make clean` and `make 3xx`, 
4. Copy the created `EBOOT.pbp` to the `bin/` folder.
5. Copy the entire `bin/` folder to `ms0:/PSP/GAME/` on your PSP (and optionally rename the directory to `CSPSP/`).

The JGE library that CSPSP uses is technically cross-platform, and during the original development I primarily ran on Windows XP with Visual Studio 2005. However, I have not tried this recently; the VS solution/project files are provided as-is and will likely require some fixing up.

**Important**: If you are making any incompatible changes to the networking (for example, adding or changing message types), please update the `NETVERSION` define in [jge/Projects/cspsp/src/GameApp.h](jge/Projects/cspsp/src/GameApp.h).

## Ports

Other developers have also made some really cool ports of CSPSP to other platforms!

- [CSWEB](https://github.com/frankplus/csweb) - WebGL port by @frankplus
  - [Live Demo](https://frankplus.github.io/)
- [CS3DS](https://github.com/machinamentum/CS3DS) - Nintendo 3DS port by @machinamentum
