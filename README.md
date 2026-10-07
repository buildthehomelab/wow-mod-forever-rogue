# Forever Rogue

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that brings WoW Forever's
rogue changes to a 3.3.5 server. For now that's combo points that follow the rogue: unused combo
points move to your next target instead of being lost. No client patch.

## Combo points follow you

In stock 3.3.5, combo points belong to one target. Attacking another target starts over at zero,
and killing the target loses them. With this module:

- **Switching targets** moves your combo points to the new target, with the full count. Select
  any hostile target you can attack and the points go there. The target frame shows them right
  away.
- **Killing the target** keeps the points for 20 seconds. The next hostile target you select or
  attack gets them.
- **A builder that kills** (a Sinister Strike that finishes the mob, say) keeps the points you
  had plus the ones it just gave.
- **Builders on a target you haven't selected**, through a mouseover or focus macro, take the
  points along too.
- Selecting a friendly player or nothing leaves the points where they are.

These still lose your combo points, as in stock:

- Using a finisher, including one that kills its target.
- Dying.
- A duel ending, when the points were on your opponent.
- Premeditation's points running out unused.

Only rogues get this. Druids in Cat Form keep stock combo points.

## Requirements

- [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) `master` (WotLK 3.3.5a)
- A WoW 3.3.5a (12340) client
- No client patch and no SQL

## Install

Clone it into your AzerothCore `modules` folder **as `mod-forever-rogue`**, without the repo's
`wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-forever-rogue.git mod-forever-rogue
```

Rebuild the worldserver, then copy `conf/mod_forever_rogue.conf.dist` to your config folder as
`mod_forever_rogue.conf`. There's no SQL.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `ForeverRogue.ComboPoints.Enable` | `1` | Master switch. With `0`, combo points work as in stock 3.3.5. |
| `ForeverRogue.ComboPoints.KeepAfterKill` | `20000` | How long, in milliseconds, points from a target that died wait for your next target. `0` loses them when the target dies; switching between living targets still carries them. |

Both can be changed with `.reload config`.

## How it works

The server keeps a rogue's combo points as a count plus the one target they're on, and tells the
client which target that is. The module checks each rogue on every server update: when the
selected target is a living hostile one without the points, it moves them there. The 3.3.5
client already shows combo points only for the target the server names, so the target frame and
addons show them on the new target without a client patch.

When a target dies, the core clears the combo points on it. The module notes the count it had
just before, and gives it to the next target. It tells a death apart from a finisher by watching
for finisher casts, so a killing Eviscerate doesn't leave points behind.

## Limits

- The move happens on the next server update after you change targets, a few dozen
  milliseconds. A finisher pressed in the same instant as the target change can still say "That
  ability requires combo points"; press it again.
- A finisher aimed at a target you haven't selected (a mouseover or focus macro) only works if the
  points are already there, because the core checks combo points before any module sees the cast.
  Builders don't have this problem.
- Playerbots rogues get it too, since they're players.

## Troubleshooting

- **A finisher says "That ability requires combo points" right after a target change.** The points
  move on the next server update, a few dozen milliseconds later. Press the finisher again.
- **A finisher on a mouseover or focus target doesn't work.** The core checks combo points before
  the module sees the cast, so the points must already be on that target. Builders aren't affected.
- **Combo points are lost.** Using a finisher (even one that kills), dying, a duel ending with the
  points on your opponent, and Premeditation's points running out all clear them, as in stock.

## Credits

Author: [buildthehomelab](https://github.com/buildthehomelab)

The design follows the WoW Forever private server ruleset. The code is original.

## License

MIT. See [LICENSE](LICENSE).
