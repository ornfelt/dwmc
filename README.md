<p align="center"><img src="dwmc.png" alt="dwmc"></p>

# dwmc - dynamic window manager

dwmc is [dwm](https://dwm.suckless.org) (6.8) with my patches, ported back to
C from dwmr, their Rust port. It is a function-for-function replica of dwmr
(and so of dwm.c/drw.c) with the same behaviour, the same monocle and
floating layouts, the same tags, bar, key and mouse bindings, plus the
gap-aware tiled layouts of dwm's vanitygaps patch. Unlike dwmr, and like dwm,
it is configured through a `config.h` that is compiled in.

## Requirements

- A C99 compiler and make
- Xlib, Xft, Xinerama, XRes and fontconfig headers (Debian: `libx11-dev
  libxft-dev libxinerama-dev libxres-dev libfontconfig-dev`)

## Installation

    make                    # builds dwmc with ~/.config/dwmc/config.h, or config.def.h
    sudo make install       # /usr/local/bin/dwmc and the man page
    make install-config     # copy my config (config/config.h) to ~/.config/dwmc/config.h

`make install PREFIX=$HOME/.local` installs without root. Under `sudo` the
Makefile still builds with the invoking user's `~/.config/dwmc/config.h`.

## Running

Add this to `~/.xinitrc` (as the last, foreground command) and run `startx`:

    exec dwmc

`dwmc -v` prints the version.

## Configuration

dwmc is configured like dwm, by editing a header and rebuilding. `make`
copies `~/.config/dwmc/config.h` to `config.h` in the source directory, or
`config.def.h` when there is none, and compiles it in. After editing
`~/.config/dwmc/config.h`, run `make && sudo make install` and restart dwmc.

`config.def.h` holds the defaults, dwm's config.def.h plus the options of the
patches. `config/config.h` is my own configuration (my dwm config.h, which
dwmr ships as `config/config.toml`).

A `config.h` that would crash dwmc does not build: more than 31 tags and
scratchpads together, no tags, no fonts or no layouts fail with a negative
array size error that names the problem (`limitexceeded`, `nofonts`,
`nolayouts`). What the compiler cannot check is checked where it is used: the
gaps are limited to 1000 pixels, a `refreshrate` below 1 counts as 1, and
`togglescratch` ignores a scratchpad that does not exist.

### X resources

At startup dwmc reads the X resources of the display (what `xrdb` loads from
`~/.Xresources`) and applies these, if present, on top of the `config.h`
values, like dwm's xresources patch; the `resources[]` table in `config.h`
lists them:

| resource                                                               | sets                                                     |
|------------------------------------------------------------------------|----------------------------------------------------------|
| `dwm.color0`                                                           | norm border, norm bg, sel fg (a `#RRGGBB` color)         |
| `dwm.foreground`                                                       | sel border, norm fg, sel bg (a `#RRGGBB` color)          |
| `dwm.borderpx`, `dwm.snap`, `dwm.gappih`, `dwm.gappiv`, `dwm.gappoh`, `dwm.gappov`, `dwm.nmaster` | integers                     |
| `dwm.showbar`, `dwm.topbar`, `dwm.resizehints`, `dwm.swallowfloating`, `dwm.smartgaps` | integers, 0 is false            |
| `dwm.mfact`                                                            | a float                                                  |

The selected foreground and background are inverted on purpose, so the
selected tag shows reversed. A value that does not parse is ignored. The
color resources are written into the `normbgcolor[]`.. buffers, so those must
stay `"#RRGGBB"` strings. The resources are read once at startup.

A few notes on the configuration:

- `TAGKEYS(KEY, TAG)` expands to my bindings, not dwm's: `MODKEY` views the
  tag, `+Control` tags the window, `+Shift` tags it and views the tag
  (`tagview`), `+Control+Shift` toggles the view.
- Two monitors split the tags: the odd tags (1, 3, 5, ...) live on the first
  monitor and the even tags on the second, which starts on tag 2. `view`
  focuses the tag's monitor first, `tag` and `tagview` move the window over
  when the tag belongs to the other monitor, `sendmonview` only gives a
  window tags of the target monitor's parity, and a monitor plugged in takes
  over the windows that have only even tags. A mask with any odd tag, like
  all tags, counts as odd. Pressing the key of the current tag returns to the
  previous tag only with one monitor.
- Functions: spawn, togglebar, focusstack, focusurgent, pushstack, incnmaster,
  setmfact, zoom, view, killclient, setlayout, cyclelayout, layoutmenu,
  togglefloating, togglefullscr, togglesticky, togglescratch, tag, tagview,
  focusmon, focusnthmon, tagmon, tagmonview, tagnthmonview, togglebars,
  toggleview, toggletag, shifttag, shiftview, shiftviewclients, defaultgaps,
  incrgaps, togglegaps, togglebgaps, sigstatusbar, quit, movemouse,
  resizemouse.
  Layouts: spiral, tile, bstack, dwindle, deck, monocle, centeredmaster,
  centeredfloatingmaster, and `NULL` for floating.
- `tagmonview` sends the focused window to the previous/next monitor like
  `tagmon`, then focuses it there and warps the pointer to that monitor.
  `focusnthmon` and `tagnthmonview` do the same as `focusmon` and
  `tagmonview` for monitor number `{.i = n}` (0 is the first; a number past
  the end means the last). `togglebars` toggles the bar on every monitor at
  once, `togglebar` only on the focused one.
- `cyclelayout` sets the layout `{.i = n}` places after the current one in
  `layouts[]`, wrapping around (dwm's cyclelayouts patch). `layoutmenu` runs
  a command, `{.v = layoutmenucmd}`, that prints the index in `layouts[]` of
  the layout to set, with the current layout's index in
  `LAYOUT_MENU_CURRENT`, and waits for it like for a menu (dwm's layoutmenu
  patch). cyclelayout is bound to the mouse wheel on the layout symbol,
  layoutmenu to a click on it and to Mod-r; my layoutmenucmd is
  `layout_menu.sh`, a rofi grid with an ascii preview of each layout.
- With `layoutalltags = 1` (the default) setlayout sets the layout of every tag
  and monitor; with 0 each tag keeps its own and a monitor shows the one of
  its first viewed tag. `togglelayoutalltags` (Mod-Shift-r) switches between
  the two at runtime; switching back to all tags gives every tag the current
  layout.
- `scratchpads[]` is a list of `{ name, cmd }` (dwm's scratchpads patch). Each
  scratchpad owns a tag bit above the normal tags, `SPTAG(0)`, `SPTAG(1)`,
  ... in a rule's tags mask; the tags and the scratchpads together are
  limited to 31. `togglescratch` with `{.ui = n}` shows or hides the window
  on scratchpad `n`, spawning its `cmd` when there is none yet. A floating
  scratchpad window is centred each time it is shown. The bar shows only the
  normal tags.
- `focusstack` and `pushstack` take a stack position (dwm's stacker patch):
  `{.i = INC(+1)}` is relative to the focused window, `{.i = 0}` is the top
  of the stack, `{.i = -1}` the bottom, another integer an absolute
  position. `pushstack` moves the focused window to that position.
- `shifttag`, `shiftview` and `shiftviewclients` (dwm's shift-tools patch)
  take `{.i = n}`: the viewed normal tags are circularly shifted by `n`
  positions (left for positive, right for negative, wrapping at the number
  of tags). `shifttag` moves the focused window there, `shiftview` views it
  and `shiftviewclients` keeps shifting until it reaches a tag that has a
  window, ignoring windows on scratchpad tags. Bound by default to
  Mod1-Shift-y/o, the mouse wheel on the tag bar and Mod1-(Shift-)Tab; the
  latter replaces dwm's Mod1-Tab "view previous tags".
- The status text (the root window name, up to 1023 bytes) is shown on every
  monitor and may contain dwm's status2d colour codes, `^...^`, which are
  not drawn: the text starts in `col1`; `^3^`..`^6^` switch to
  `col3`..`col6`, `^c#rrggbb^` to any colour and `^2^` to the weather colour
  chosen from the temperature after it (+20 and above `col21`, below +20
  `col22`, negative `col23`, no sign `col24`). `^B^` switches to the bigger
  `statusbigfonts` until `^N^`, e.g. for a block's icon. Other codes (`^r`,
  `^b`, `^d`, `^f`) are ignored; an unterminated `^` ends the text.
- Clickable status blocks (dwm's statuscmd patch): a status bar like
  dwmblocksc or dwmblocks puts its block's signal as a byte (1..31) before
  each block. These bytes are not drawn. A click on the status text runs the
  button's function with that block's signal remembered, and `sigstatusbar`
  sends it to the program named `statusbar` (default `"dwmblocksc"`, found
  by process name) as SIGRTMIN+signal with the `.i` as value; the program
  then runs the block's command with `BLOCK_BUTTON` set to it. By default
  Button1..3 on the status text send 1..3.
- `autostart` is a command (an argv array) that dwmc runs once at startup,
  after the existing windows have been taken over, without waiting for it.
  The default `{ "sh", "-c", "killall -q dwmblocksc; dwmblocksc &", NULL }`
  (re)starts the status bar, so `dwmblocksc` needs no separate autostart.
  `{ NULL }` runs nothing; `statusbar[] = ""` disables the clicks.
- `{.v = termcmd}` spawns a command array; `{.v = &layouts[2]}` selects an
  entry of `layouts`. When `dmenucmd` is spawned, `dmenumon` is set to the
  selected monitor.
- The tiled layouts (spiral, tile, bstack, dwindle, deck, centeredmaster,
  centeredfloatingmaster) leave gaps between windows (`gappih`, `gappiv`) and
  at the screen edge (`gappoh`, `gappov`), 20 pixels each by default, at most
  1000 (dwm's vanitygaps patch, without cfacts: windows in an area share it
  evenly). `smartgaps = 1` drops the outer gaps for a single window. A
  single Firefox window (class starting with "firefox", any case) gets no
  outer gaps unless `browsergaps = 1`. `incrgaps` with `{.i = n}` changes
  all gaps of the selected monitor by `n`, `defaultgaps` resets them,
  `togglegaps` turns gaps off and on and `togglebgaps` toggles `browsergaps`.
  Bound to Mod1-plus/minus (3 px), Mod1-Shift-plus/minus (1 px), Mod1-x,
  Mod1-z and Mod1-Control-z, and Mod1-Button2/4/5 on a window (defaultgaps,
  +1, -1), which replaces dwm's Mod1-Button2 togglefloating. The default
  layout is spiral; Mod1-t/f/m still select tile, floating and monocle.
- The bar shows no window title and has no title click area (dwm's notitle
  patch); there is no `ClkWinTitle`.
- Only selected or occupied tags are drawn, without the small occupancy
  squares (dwm's hide_vacant_tags patch). A window on every tag does not
  count as occupying them. Clicks on the tag bar follow the same layout.
- The only tiled window on a monitor, and every window in the monocle layout,
  is drawn without a border (dwm's noborder patch).
- Focus follows mouse clicks only, not pointer movement (dwm's focusonclick
  patch). `focusonwheel = 0` (the default) lets the scroll wheel work on an
  unfocused window without focusing it.
- A sticky window is visible on every tag of its monitor (dwm's sticky
  patch). `_NET_WM_STATE_STICKY` is supported, so a client can set the state
  itself, and `_NET_WM_STATE` lists both the fullscreen and the sticky state.
- A window toggled back to floating returns to the position and size it last
  floated with (dwm's savefloats patch). A window that never floated, or whose
  saved position is on another monitor, is centred on its monitor instead
  (dwm's togglefloatingcenter patch).
- A window started from a terminal takes the terminal's place, and the
  terminal comes back when the window closes (dwm's swallow patch). A rule
  marks a window as a terminal with `isterminal = 1`; `noswallow = 1` keeps a
  window from swallowing its terminal. Floating windows only swallow with
  `swallowfloating = 1`. The terminal is found through the window's PID (from
  the X-Resource extension) and its parents in `/proc`.
- Every colour gets an opaque alpha byte, so the borders of 32-bit (ARGB)
  windows do not turn transparent under a compositor like picom. This is not
  dwm's alpha patch: the bar and the windows keep the default visual.

## Layout of the source

- `dwm.c` - dwm.c: dwmr's `src/dwm.rs` and `src/main.rs`, with the config
  types of `src/config.rs`. `vanitygaps.c` - the gaps and the gap-aware
  layouts, `#include`d by dwm.c.
- `drw.c`, `drw.h` - drw.c, the bar drawing and font fallback code.
  `util.c`, `util.h` - util.c.
- `config.def.h` - the defaults (dwmr's `Config::default()`);
  `config/config.h` - my config (dwmr's `config/config.toml`).
- `transient.c` - a test client (`make transient`).
- `test.c` - the unit tests.

Xinerama support is `XINERAMAFLAGS` in `config.mk`, like dwm:
`make XINERAMAFLAGS= XINERAMALIBS=` builds without it.

`make test` runs the unit tests under AddressSanitizer and
UndefinedBehaviorSanitizer and builds dwm.c against both `config.def.h` and
`config/config.h`. `make debug` builds `dwmc-debug`, dwmc with the same
sanitizers. Run it with
`ASAN_OPTIONS=fast_unwind_on_malloc=0 LSAN_OPTIONS=suppressions=tools/lsan.supp`:
`tools/lsan.supp` suppresses fontconfig's own configuration, which nothing
frees, and nothing of dwmc.

`dwmc.png` is drawn by `tools/logo.py`.

## License

MIT/X Consortium License, see LICENSE. dwm is (c) its authors; see LICENSE
for the full list.
