# Changelog

## v2.4.1 – Fixes for roadblocks, cops and controllers

- **Controller player could only reverse** (one player on a controller, the other on the keyboard): each player's car controls went through the controller layout picked in single player Options → Controls (PlayerSettings, InputPlayer::FetchInput -> GetControllerAttribs). With a non-default layout, accelerate came from the wrong input. Split screen players now always use the default layout; the mod's own controls (2P CONTROLS) decide the buttons.
- **Roadblocks disappearing:** cops standing in a roadblock were taken off the road by the out-of-sight clean-up, reused for new cops and roadblocks by the game (GetActiveCopVehicleFromOutOfView), or sent off after a bust, and a roadblock without cops is removed. Roadblock cops now stay.
- **Roadblocks get their own cars:** the 13-cop limit counts only cops chasing (not roadblock cars or cops driving off), and a roadblock only borrows cops left 300 m+ behind everyone, never chasing ones.
- **Cops vanishing:** a cop is only taken off the road when also 120 m+ from both players, a busted player included.
- **The cop manager could stop mid-race** (no roadblocks, Rhinos or the game's own cops until a restart). The mod notices when it stands still for 2 s and keeps it running.
- More roadblocks get placed: each try looks 250, 300, 350, 200 or 400 m ahead instead of always 250 m.
- **Minimaps disappeared when player 1 looked back** (the game hides a player's HUD while looking back, and both minimaps come from player 1's HUD). In split screen the HUD and minimaps now stay on while looking back.

## v2.4 – Roadblocks and spike strips are back

- **Roadblocks and spike strips** in split screen races with cops, taking turns with Rhinos as backup:
  - One on the road at a time, the next at least 25 s later. The game places it 250 m ahead on the race route.
  - Players under 1 km apart (race route metres): it's set up ahead of the leader and stays until both have passed it. When the leader gets through, it's handed to the other player's pursuit (IPursuit::AddRoadBlock + IRoadBlock::SetPursuit, as CreateRoadBlock does), so the game keeps it up for them.
  - Further apart: each player in turn, ahead of themselves. One player out: the one still racing. A roadblock left without a pursuit goes to whoever still has to pass it, else its cops leave once out of sight.
- **Cop cars for roadblocks:** the game has about 18 cop cars. The chase now uses at most 13 on the road (the mod's extra cops and the game's own), so 5 stay free for roadblocks, spike strips and Rhinos. Racing together, nearly every roadblock used to be dropped for lack of cars. Before a block is due, cops out of sight are recycled sooner (150 m instead of 300 m).
- **Cops is now Off / On** on the race options screen: On is Heat 5 (the lower heat levels are gone). A saved heat level counts as On.
- **Near camera** for every player when split screen starts; player 1's own (single player) camera comes back on the Quick Race menu.
- The BUSTED / OUT OF THE RACE screen now covers the busted player's HUD and minimap (still under the pause menu).
- The log now shows the game's free memory every 30 s and at a crash. A long race with cops crashed a 2 GB `speed.exe` (out of memory while loading the world); see the README for the 4 GB recommendation.
- 4 Player Split Screen is hidden until its own release (no cops in it, and players 3 and 4 have their own camera, for when it comes).

## v2.3.1 – Cops drive off instead of vanishing

- **Cops no longer disappear in front of you.** When a player is busted, the cops far from the other player turn their sirens off and drive away (the game's own AIGoalFleePursuit, as when the heat level goes up in single player) instead of being removed on the spot.
  - Cop cars are only taken off the road once they're out of sight: cops driving off after 2 s out of sight and 120 m from every player, others after 3 s out of sight and 300 m from every player still racing.
  - Over the game's 8-car cap, a cop out of sight is removed; otherwise the cop farthest from both players drives off (the game judged this from player 1's camera only and removed it on the spot).
  - Cops driving off don't count as chasing a player, so backup is still sent on time.
- **4 Player Split Screen:**
  - New menu icon (four numbered players in a 2×2 split), drawn by `tools/gen_icon4p.py`.
  - Players 3 and 4 choose on the game's car select with PLAYER 3 / PLAYER 4 in its title (the mod's banner is gone).
  - The race options screen has no AI racers, AI difficulty, catch up or cops rows; with 3 players the 4th car is still an AI racer.

## v2.3 – Equal cops, 4 players (beta)

- **Cops chase both players equally.** Before, nearly all new cop cars went to player 1 and player 2's spawned behind them or next to player 1. Now:
  - Each spawn is checked against the camera of the player it's for, and new cars are shared between the two pursuits.
  - More cop cars: twice the heat level's count plus one at Heat 4–5, extra cars sent when a player has fewer than 7 cops close (Heat 5), up to 32 cop cars at once.
  - New cars come from ahead (340 m). Within 400 m of each other: ahead of the leader; further apart: ahead of each player. Never dropped in right in front of the player behind.
  - When a player is busted, the cops near the other player join their chase and the ones far behind are taken off the road; cops can't start new chases on a player who is out.
  - Cop cars stranded far behind both players are recycled.
- Roadblocks and spike strips are off for now (being reworked); Rhino SUVs come every 20 s at Heat 5 (+10 s per lower heat), and P2's arrive ahead of P2.
- Fixed: the screen flashing blue after a bust (a new pursuit started and ended on the busted player every frame).
- Fixed: the results screen opening twice.
- **4 Player Split Screen (beta)**: new Quick Race entry for 3–4 players (AI 4th car with 3), with HUDs, world streaming, full car sounds, effects and camera shake for players 3 and 4, and correct standings (also in lap knockout). No cops or minimaps with 4 players.

## v2.2 – Pursuit

- **Cops in split screen races.** New COPS option on the race options screen: Off or Heat 1–5, in every race mode including Lap Knockout.
  - The cops chase both players with sirens; cop cars blink red/blue on both minimaps.
  - Backup in a fixed order: roadblock, spike strips, Rhino SUVs head-on. Every 20 s at Heat 5, +10 s per lower heat. Roadblocks stay on the race route.
  - Busted meter above each player's speedometer while they are in a pursuit.
  - A busted player is out of the race: the game's BUSTED banner, then a BUSTED / OUT OF THE RACE screen on their half and their car is taken off the road. The other player races on and the cops switch to them.
  - Finished or knocked-out players are left alone; the cops go after whoever is still racing.
  - When both players are out the results open, with busted players ranked last as BUSTED.
- Fixed: crash on restart (the minimap was used after the game deleted it).
- Fixed: crash with many cops on screen (the game's per-frame render memory ran out; it is now 4 MB instead of 400 KB).
- Restart resets everything cop-related (busted screens, timers, pursuits).

## v2.1

- Player 2 can now pick from player 1's career cars and My Cars in split screen car select, with their paint, body kits and performance parts. Turn it off with `Player2UsesPlayer1Cars=0`. Your save is not changed.

## v2.0

- Lap Knockout in split screen. Laps work like single player: the last racer each lap is knocked out and gets the game's own KNOCKED OUT screen. A knocked-out player's car stops while the other player keeps racing.
- The game's race over message (winner text) now shows for each player before the results screen, which waits `ResultsDelayMs` (default 4000 ms).
- The red flares at the finish line now show in split screen in every race mode.
- All world models are drawn in split screen.

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
