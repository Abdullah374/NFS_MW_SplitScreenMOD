# Changelog

## v1.9

- Race options in split screen: traffic level, 0–4 AI racers, AI difficulty and catch up. Difficulty and catch up are greyed out when there are no AI racers.
- AI racers spawn in split screen races and appear as red arrows on both minimaps (colour: `AIMinimapColor`).
- Starting grid: two cars per row behind the start, in a random order with the players included (`RandomStartGrid`), all facing forward. The grid is also used on restart.
- Speed trap races: a SCORE row under COMPLETE on each player's HUD (`ShowSpeedTrapScore`).
- Speed trap minimap icons are per player: a trap only disappears from the minimap of the player who drove through it.
- Removed the PLAYER 1 / PLAYER 2 car select banner (the game's car select screen already shows whose turn it is).
- Release settings ship with `DebugLog=0`.

## v1.8

- Join screen, per-player saved controls (controller + keyboard), 2P CONTROLS in the pause menu.
- Both players can navigate menus and the pause menu.
- Separate minimaps, world streaming for player 2, 1080p / Widescreen Fix support.
