# dwmc / dwmblocksc port status

Maintained by the `dwmc-port` skill. A hint for the next run, not the source of truth - the
trees are. Re-derive with: `grep -rn 'TODO: port body' . ../dwmblocksc --include='*.[ch]'`

**Last run:** vanitygaps.c, man page, README, functional tests, `~/.config/dwmc/config.h` (groups 9-10) - dwmc is complete
**Next:** dwmblocksc (group 11), in one run

## Porting order

- [x] 1. setup - Makefile, config.mk, LICENSE, README.md, .gitignore, logo (`tools/logo.py`, `dwmc.png`)
- [x] 2. util - `util.c`/`util.h`
- [x] 3. drw - `drw.c`/`drw.h`
- [x] 4. transient - `transient.c`
- [x] 5. config - types in dwm.c, `config.def.h`, `config/config.h` (both verified against dwmr, see below)
- [x] 6. dwm.c skeleton - every function a stub, `vanitygaps.c` stubbed too (config.h needs its layouts)
- [x] 7. dwm.c, first half - `applyrules` .. `manage`
- [x] 8. dwm.c, second half - `manage` .. `zoom`, then `main()`
- [x] 9. vanitygaps - `vanitygaps.c`
- [x] 10. man page (`dwmc.1`), README, tests, functional test, `~/.config/dwmc/config.h`
- [ ] 11. dwmblocksc

The gcc/clang builds warn only about `quit` being unused with
config/config.h (and so with `~/.config/dwmc/config.h`, a copy of it), which
has no quit binding (dwmr's shipped config neither: the powermenu exits).
The sanitized builds (`make test`, `make debug`) with gcc also warn "null
format string" in `die()`: a gcc false positive under -fsanitize, nothing
passes NULL. `-Wno-unused-parameter` is in config.mk: dwm's handlers ignore
their `arg` everywhere, and dwm builds without -Wextra.

## Decisions the port follows

- **Status drawing** (drawstatusbar, buttonpress): no allocation. The
  signal-byte-free copy of `stext` is a local `char[sizeof stext]`, not
  dwmr's `stextdraw` field or status2d's malloc. The status colors are
  `statusclr[ColLast]` (enum `Col1`..`Col6` in dwm.c), allocated once in
  setup with `drw_scm_create` from config.h's `col1`..`col6`; `^c#rrggbb^`
  goes through `statuscolor()` and the `statusclrs[STATUSCLRS]` cache.
  `weathercolor()` and `statuscolor()` return `Clr *`, and the codes set
  `drw->scheme[ColFg]` directly like status2d. That must not change an
  owned scheme: dwmr's drw copies the scheme on `setscheme` and its
  `SCHEME_STATUS` is a separately allocated copy of SchemeNorm, so draw the
  status with a local copy (`Clr[3]`) of the norm/status colors.
- **Fonts**: `normalfont`/`statusbigfont` pointers as in the user's status2d
  dwm.c; dwmr's `setnormalfont()`/`bigfont` swap become
  `drw_setfontset(drw, normalfont)`. `cleanup()` must restore `normalfont`
  before `drw_free()` and free `statusbigfont` itself.
- **Checks the compiler cannot do** (dwm.c says so after `#include
  "config.h"`, README promises them): `createmon()` clamps the gaps to
  `GAP_MAX` (X resources may set any value); movemouse/resizemouse divide by
  `MAX(refreshrate, 1)`; `togglescratch()` returns for `arg->ui >=
  LENGTH(scratchpads)`; `spawn()` returns when `argv[0]` is NULL (dwmr: an
  empty command).
- **Masks are unsigned** (`TAGMASK`, `SPTAG`, `TAGBITS`, `SCREEN_MASK` use
  `1U`), like dwmr's u32: `1 << 31` would be undefined.
- **dwmr's atoi/strtoul/strtof are dropped** (libc in C), but keep their
  semantics where dwmr relied on them: `weathercolor()` uses `strtol` (dwmr's
  atoi saturates; libc atoi overflow is undefined); `resource_load()` sets
  nothing when no digit was parsed (check `endptr`), where dwm's patch would
  store 0.
- **Signed arithmetic**: drw_text centres with signed ints (done). Where
  dwmr's `+` could overflow an int (`incnmaster` with an X-resources nmaster,
  `incrgaps`: dwmr uses `saturating_add`), clamp in C. `mfact` can be any
  float from config.h or X resources (dwmr's `as i32` saturates; C's
  float-to-int conversion of an out-of-range or NaN value is undefined), so
  the layouts must clamp before converting; `setmfact()` must reject NaN like
  dwmr's `contains()` (`if (!(f >= 0.05 && f <= 0.95)) return;`).
- **vanitygaps**: no `layoutarea()` (a Rust helper for reading monitor
  fields): read `m->wx`.. directly and clamp nmaster to `0..n` inline where
  dwmr's layoutarea did. `getgaps()` counts in an `int` (dwmr's i32), not the
  patch's unsigned. `GAP_MAX` is defined in vanitygaps.c.
- **gettextprop**: fill the buffer with `truncate_utf8()` (dwmr cuts at a
  character boundary); invalid UTF-8 stays as bytes (dwmr converts it
  lossily; drw_text draws U+FFFD per invalid sequence either way).
- **Names**: the wmcheck window's `_NET_WM_NAME` is "dwmc", the bar's class
  stays "dwm"; messages say "dwmc:"; `updatestatus()`'s default text is
  "dwmc-" VERSION.

- **mfact to int** (vanitygaps.c): `ftoi()` converts the mfact products
  like dwmr's `as i32` (NaN is 0, saturating), but at half the int limits
  so the int arithmetic after it cannot overflow (dwmr wraps there); it
  differs from dwmr only for mfact values far outside 0..1. `incrgaps()`
  limits its step to `GAP_MAX`, which gives what `saturating_add` gives
  after `setgaps()` clamps.
- **vanitygaps** reads `browsergaps` (config.h) as its runtime state, which
  `togglebgaps()` flips (dwmr copies it into a field since its config is
  immutable), and `enablegaps` (vanitygaps.c).

## Known analyzer report

`scan-build` reports one "use of memory after it is freed" at `cleanup()`'s
`unmanage(m->stack, 0)`: the analyzer cannot know that every client in
`m->stack` has `c->mon == m`. Stock dwm 6.8 gets the same report at the same
line (checked); its other three reports (division by zero in tile, NULL `m`
in updategeom/cleanupmon) do not occur in dwmc.

## Rust issues found (ported faithfully or noted, not fixed in dwmr)

- `pushstack()`: with a plain index past the last visible client whose last
  client is sel, the walk ends on sel and sel is linked after itself (dropped
  from the client list). The comment says it is a no-op; it is not.
  Unreachable with the shipped keys (INC(), 0, -1). Ported as is.
- `unmanage()`: when a swallowed terminal's window is mapped again and
  managed as a second client, unmanaging that client frees the listed client
  (`free_client(c)`) instead of the swallowed one. dwmc frees
  `s->swallowing`, as the swallow patch does; identical in every other case.
- `gettextprop()`: an empty property's value is not XFree'd (dwm has the same
  leak); dwmc frees it.

## Verification record

- config/config.h and config.def.h were compared with dwmr by dumping both
  C headers (compiled against dwm.c's types) and dwmr's config.toml (and a
  transcription of `Config::default()`) through dwmr's parse rules into the
  same text form: identical, including all 144 keys, 19 buttons and 8 rules.
- Negative builds: 30 tags + 2 scratchpads, empty `fonts[]`, empty
  `layouts[]` fail to compile; 29 + 2 builds.

- Functional test A (Xvfb, `make debug` with config.def.h): spawn,
  focus/push stack, zoom, every config.def.h layout, gaps keys, nmaster,
  mfact, float, fullscreen, sticky, tags, shiftview, both scratchpads,
  swallow (the terminal is unmapped and comes back), clicks on tags, layout
  symbol and status (the signal reaches the status bar by argv[0]), kill,
  togglebar; `alt+shift+q` exits 0 with no ASan/UBSan/LSan report (LSan run
  with `fast_unwind_on_malloc=0` and `tools/lsan.supp`, which suppresses
  only fontconfig's configuration loaded by XftInit).
- Functional test B, dwmr (`config/config.toml`) on :98 against dwmc-debug
  (`config/config.h`) on :99, the same 31-step xdotool sequence (4
  terminals, all 8 layouts, gaps, focus, zoom, float, fullscreen, tags,
  scratchpad, clicks on tag bar and status, kill): `xwininfo -root -tree`
  and the `_NET_*` root properties identical at every step (window ids
  masked, the wmcheck window's name aside), the bar pixel-identical.
  Test stubs in PATH: `killall` (a no-op, so the live dwmblocksr survives),
  `wezterm` (xterm with that class), the status bars (`xsetroot` a fixed
  text, then `exec -a <name> sleep`); dwmr's copy of config.toml names the
  status bar `dwmblocksx`, so no click signals the live dwmblocksr.
- `dwmc` on the live display: "dwmc: another window manager is already
  running", exit 1; `-v` and usage as dwmr's (exit 1).

## Run log

| Run | Files | ~C lines | Build | Tests |
| --- | --- | --- | --- | --- |
| 1 | setup, util.c/h, drw.c/h, transient.c, dwm.c + vanitygaps.c skeleton, config.def.h, config/config.h, test.c | 2750 (900 of them stubs) | gcc, clang, no-Xinerama, both configs: only stub warnings; scan-build clean | make test (util, drw) passes under ASan+UBSan |
| 2 | dwm.c: all functions and main() | 2000 | gcc, clang, no-Xinerama, both configs: only vanitygaps-stub warnings (+ `quit` with config/config.h); scan-build: the one report stock dwm has too | make test passes; no functional test yet (layouts are stubs) |
| 3 | vanitygaps.c, dwmc.1, README, tools/lsan.supp, `~/.config/dwmc/config.h` | 470 + 400 man | gcc, clang, no-Xinerama, both configs: only `quit` unused (config/config.h); scan-build: the known report | make test passes; functional tests A and B pass (Xvfb) |
