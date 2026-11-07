THALASSA - test build
Beep. Bloop.

This is an early prototype: a grey-box ship you can walk around, with an airlock
and the sea outside. It is for testing, not a game yet.

PLAYING TOGETHER ON ONE NETWORK
1. On the first PC, double-click Host.bat. It shows this PC's address and starts
   the game. Allow it through the Windows firewall if asked (private networks).
2. On the second PC, double-click Join.bat and type the address shown on the host.

Both PCs need this whole folder. Nothing needs installing.

PLAYING OVER THE INTERNET
The game cannot yet connect over the internet by itself. Until it can, the host
can use a tunnelling service such as playit.gg, and nobody else installs anything:
1. The host creates a UDP tunnel to local port 7777 and runs Host.bat.
2. Everyone else runs Join.bat and types the tunnel's address and port,
   for example example.playit.gg:12345.

CONTROLS
  W A S D, mouse    walk and look (in water: swim where you look)
  Space             jump (in water: up)      C   in water: down
  E                 use the bridge console, or cycle the airlock
  Tab               expanded station view
  I / K             ship forward / reverse
  J / L             turn       U / O   dive / climb
  P                 run the scripted motion test
  Alt+F4            quit

IF SOMETHING GOES WRONG
Logs are in Thalassa\Saved\Logs inside this folder. Send Thalassa.log from both PCs.
