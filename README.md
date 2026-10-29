# NFS Most Wanted (2005) – Split Screen

Local 2-player split screen for the PC version of *Need for Speed: Most Wanted* (2005).

The console versions of Most Wanted had a split screen Quick Race mode. Most of that code is still inside the PC `speed.exe`, but it was switched off. This mod (an `.asi` plugin) turns it back on and adds what the PC version was missing.

**This repository contains no game files.** You need your own copy of the game.

## Features

- **Split Screen** option in the Quick Race menu
- Top/bottom (default) or side-by-side layout
- Each player gets their own camera and HUD
- The world loads around both players, however far apart they are
- A separate minimap for each player; each map also shows the other player's arrow
- A join screen before car selection: the first device to press A (or Enter) is Player 1, the second is Player 2
- Clear "PLAYER 1 / PLAYER 2 – CHOOSE YOUR CAR" banners on the car select screens
- Each player can use any XInput controller or the keyboard. Two players can share one keyboard.
- Remappable controls for each player, set in-game: **pause → Options → 2P CONTROLS**. Each action has a controller and a keyboard binding, saved per player, whichever device they join with.
- Both players can navigate the menus, including the pause menu
- Works with ThirteenAG's Widescreen Fix (1080p and other widescreen resolutions)

## Quick install

1. Download **NFSMW-SplitScreen-v1.8-release.zip** from [Releases](../../releases).
2. Download **Ultimate-ASI-Loader.zip** from [Ultimate ASI Loader releases](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases).
3. Copy `dinput8.dll` (from the ASI Loader zip), `NFSMWSplitScreen.asi` and `NFSMWSplitScreen.ini` into your game folder (next to `speed.exe`).
4. Start the game, then go to **Quick Race → Split Screen**. On the join screen each player presses **A** on their controller (or **Enter** on the keyboard).

## Requirements

- NFS Most Wanted PC **v1.3** (`speed.exe`, 6,029,312 bytes, SHA-256 `80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c`)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases), as `dinput8.dll`
- Optional, recommended: [NFS Most Wanted Widescreen Fix](https://github.com/ThirteenAG/WidescreenFixesPack/releases/tag/nfsmw)
  - In `scripts\NFSMostWanted.WidescreenFix.ini`, set `SkipIntro = 0` (otherwise the game can crash at startup).
  - Also set `[CAMERA] Enable = 0` (otherwise the right stick turns the camera).

## Installation

1. Put `dinput8.dll` (Ultimate ASI Loader) in the game folder, next to `speed.exe`.
2. Put `NFSMWSplitScreen.asi` and `NFSMWSplitScreen.ini` from the [Releases](../../releases) page in the game folder.
3. Start the game and go to **Quick Race → Split Screen**.

## Controls

Defaults (change them in-game under pause → Options → 2P CONTROLS):

| Action | Controller | Player 1 keyboard | Player 2 keyboard |
|---|---|---|---|
| Accelerate | RT | Up | W |
| Brake / Reverse | LT | Down | S |
| Steer | Left stick | Left / Right | A / D |
| Handbrake | A | Space | F |
| Nitrous | B | Left Alt | Q |
| Speedbreaker | X | Right Ctrl | E |
| Shift up / down | RB / LB | Left Shift / Left Ctrl | X / Z |
| Change camera | Y | C | V |
| Look back | Right stick click | L | Tab |
| Reset car | Back | R | G |
| Pause | Start | Esc | – |

Settings are in `NFSMWSplitScreen.ini`. Each player's device and bindings are saved to `NFSMWSplitScreen_Controls.ini`.

## Building

Cross-compile with MinGW-w64 (32-bit):

```sh
i686-w64-mingw32-g++ -O2 -std=c++17 -shared -static -static-libgcc -static-libstdc++ \
    -o NFSMWSplitScreen.asi src/main.cpp -lgdi32
```

Or run `./build.sh`.

## How it works (short version)

- **Menu:** adds the cut "Split Screen" icon back to the Quick Race menu. The game's own split screen flow (2 players, split screen user mode) then takes over.
- **Memory:** the PC build reserves only 1 KB of car memory for 2-player races. The mod raises it to the 1-player size.
- **Rendering:** the PC renderer draws only player 1's view. The mod draws the scene twice per frame (player 2's camera swapped into the main view for the second pass), adjusts each projection to the half-screen size, and combines both images before the HUD is drawn.
- **Input:** the PC input system has a single game device. The mod adds input devices for both players and extends the front end to listen to more than one controller.
- **Streaming:** world streaming is fed from player 2's camera as well.
- **Minimaps:** the HUD is drawn in two passes. The minimap is re-centred on each player and moved into their half.

## Credits

- Function and data names for `speed.exe` v1.3 come from the [NFS:MW decompilation project](https://github.com/dbalatoni13/nfsmw) symbol data.
- Ultimate ASI Loader and the Widescreen Fix are by [ThirteenAG](https://github.com/ThirteenAG).
- Need for Speed: Most Wanted © Electronic Arts. This project is not affiliated with or endorsed by EA.

## License

MIT, see [LICENSE](LICENSE). This applies only to the code in this repository.
