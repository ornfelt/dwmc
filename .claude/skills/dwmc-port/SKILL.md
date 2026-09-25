---
name: dwmc-port
description: Port dwmr and dwmblocksr (the Rust ports of dwm and dwmblocks) back to C, as dwmc in Code2/C/dwmc and dwmblocksc in Code2/C/dwmblocksc. Both are configured through compiled-in headers like the originals (config.h, blocks.h), not TOML. Scaffolds both repos, then ports the next slice of Rust files to minimal, fast, crash-free and leak-free C - or, with no argument, finds where the port left off and continues it.
disable-model-invocation: true
argument-hint: "[setup, continue, dwmblocksc, skeleton <file>, implement <file>, review <file>, or a Rust file / function - empty means continue where the port left off]"
---

# Port dwmr and dwmblocksr to C

The goal is two C programs that behave **exactly** like the Rust ones:

| Rust source (read-only)                | C target (written by this skill) | executable   | user config                                   |
| -------------------------------------- | -------------------------------- | ------------ | --------------------------------------------- |
| `$code_root_dir/Code2/Rust/dwmr`       | `/home/jonas/Code2/C/dwmc`       | `dwmc`       | `$HOME/.config/dwmc/config.h` (compiled in)   |
| `$code_root_dir/Code2/Rust/dwmblocksr` | `/home/jonas/Code2/C/dwmblocksc` | `dwmblocksc` | `$HOME/.config/dwmblocksc/blocks.h` (compiled in) |

(`code_root_dir` is `/home/jonas`; use absolute paths, the trees are not reachable from each other
by a short relative path.)

Same logic, same structure, same function names, same comment style, same bar, same status
protocol - as the Rust. dwmr is itself a function-for-function replica of dwm 6.8 plus the user's
patches, and dwmblocksr of the user's dwmblocks fork, so the C ports should read like dwm.c and
dwmblocks.c again: suckless C, with the patches and fixes the Rust ports carry.

**The one deliberate difference from the Rust: both programs are configured the suckless way,
through a header compiled into the binary, not a TOML file read at startup.** dwmc uses dwm's
`config.h`, dwmblocksc uses dwmblocks' `blocks.h`. Every setting the Rust reads from `config.toml`
is a variable, array or `#define` in that header instead, and changing the config means rebuilding
and restarting, as with dwm and dwmblocks. The runtime loaders of both `config.rs` files (TOML
parsing, `parse_int_expr`, `parse_mask`, `parse_keysym`, the function-name tables, the error
messages) are not ported, and no TOML parser is used anywhere.

Read both Rust skills before the first run of a session - they describe the programs being ported
and every decision the Rust made:

- `$code_root_dir/Code2/Rust/dwmr/.claude/skills/dwmr-dev/SKILL.md`
- `$code_root_dir/Code2/Rust/dwmblocksr/.claude/skills/dwmblocksr-dev/SKILL.md`

## Priorities, in order

The same as dwmr's and dwmblocksr's, translated to what C needs:

1. **Safety: never crash, never leak.** C gives none of Rust's guarantees for free, so each one
   the Rust code relied on is written out by hand:
   - Every index that Rust bounds-checked is checked where it can be out of range (anything from
     X, a config file, command output, a signal value). Where Rust used `.get()` and skipped,
     C tests the bound and skips.
   - Every `Option` is a pointer that is tested for `NULL` before use, at the same place the Rust
     matches on it.
   - No `strcpy`, `strcat`, `sprintf`, `gets`, unbounded `%s` into a buffer. Fixed buffers are
     filled with `snprintf`, or `strncpy` plus an explicit terminator, or the ported
     `truncate_utf8()`, at the same places dwmr truncates.
   - Signed overflow is undefined in C. Where Rust used `wrapping_*`, use unsigned arithmetic;
     where it used `saturating_*` or a checked op, clamp explicitly. No division by a value that
     can be zero.
   - **Config values the Rust validated at load time are checked at compile time in C**, with
     dwm's idiom: `struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };`. Port each
     check `parse()` makes that can be expressed that way (tags plus scratchpads at most 31,
     `fonts`/`layouts` not empty, gaps at most `GAP_MAX`, `refreshrate` at least 1, scratchpad
     indices in range; for dwmblocksc: the signal byte, icon and delimiter fit in `CMDLENGTH`,
     signals at most `SIGRTMAX-SIGRTMIN` and below 32), so a bad header fails to build instead of
     crashing. What cannot be
     checked at compile time is checked where it is used, as dwm does.
   - Allocation failures: `ecalloc()` dies, as in dwm - only at startup, never in the event path,
     because the event path does not allocate.
   - **Every allocation has one owner and is freed.** Fonts, colour schemes, cursors, monitors,
     clients and the drw are released in `cleanup()`; dwmblocksc frees whatever it allocates
     (the per-block buffers, the selected block list) on exit. An ASan/LSan
     run must report no leak with a dwmc/dwmblocksc frame in it (see "Verification").
   - After `fork()` and before `exec`, only async-signal-safe calls: argv/envp are built before
     forking, as dwmblocksr does. Signal handlers only `write()` to the self-pipe (dwmblocksc) or
     set a `volatile sig_atomic_t`, and save/restore `errno`.
2. **Speed and memory efficiency.** No allocation in the event path (dwm has none; dwmr reuses
   buffers - keep that). Fixed-size buffers where dwm has them (`stext[256]`, `name[256]`,
   `ltsymbol[16]`, dwmblocks' `CMDLENGTH`/`STATUSLENGTH`). Linked lists walked like dwm walks
   them. No hash maps, no arrays that grow at runtime. Config is `static const` data. Build with
   `-Os` like dwm.
3. **Minimal.** As little code as does the job, in suckless style. No generic container library,
   no helper layers, no wrappers around Xlib, no "utils" files that have no Rust counterpart. dwmc
   links only what dwm links (Xlib, Xft, fontconfig, Xinerama) plus XRes for swallow, dwmblocksc only Xlib - no config
   parser at all. If a C construct is shorter than the Rust translation, take it: C has no borrow
   checker to work around, so the Rust workarounds (copying fields to locals, `Rc::clone` of the
   config, slab ids) disappear.
4. **Fidelity.** Same behaviour as the Rust, byte for byte where it is observable: bar drawing,
   layouts, geometry, focus order, EWMH properties, stderr messages, exit codes, `-v` output, the
   status text dwmblocksc writes, the signal byte before each block, `BLOCK_BUTTON`. Where dwmr
   deliberately deviates from dwm (and comments it), dwmc deviates the same way, with the same
   comment. The config headers are the only intended difference. Fidelity means the Rust's
   *logic*, not its code shape: the C uses C's structure and naming conventions (see "Write C,
   not Rust in C syntax").

## Sources and references

| What                                  | Where                                        | Role |
| ------------------------------------- | -------------------------------------------- | ---- |
| dwmr (Rust)                           | `$code_root_dir/Code2/Rust/dwmr`             | **the specification** for dwmc's behaviour |
| dwmr's `Config::default()`            | `dwmr/src/config.rs`                         | **the specification** for `config.def.h` |
| dwmr's shipped config                 | `dwmr/config/config.toml`                    | **the specification** for `config/config.h` |
| dwmblocksr (Rust)                     | `$code_root_dir/Code2/Rust/dwmblocksr`       | **the specification** for dwmblocksc |
| dwmblocksr's `Config::default()`      | `dwmblocksr/src/config.rs`                   | **the specification** for `blocks.def.h` |
| dwmblocksr's shipped config           | `dwmblocksr/config/config.toml`              | **the specification** for `config/blocks.h` |
| dwm 6.8 (C)                           | `~/Downloads/dwm`                            | C idiom and layout reference for dwm.c/drw.c/util.c/config.def.h |
| the user's patched dwm (C)            | `~/.config/dwm` (`dwm.c`, `config.h`, `vanitygaps.c`, `patches/*.diff`) | the patches and the config.h in their original C, which dwmr ported |
| the user's dwmblocks fork (C)         | `~/.config/dwmblocks` (`dwmblocks.c`, `blocks.def.h`, `blocks.h`, `compile.sh`) | the C and the blocks.h that dwmblocksr ported |

When the C references and the Rust disagree, **the Rust wins**: it has the bug fixes, the later
config changes (e.g. btop on mod-shift-b, the Escape-cancellable screenshot) and the deliberate
deviations (the bar's window class is `"dwm"`, the `nn == 0` case in `updategeom`,
sb-battery/sb-internet chosen at startup). The C references are there so that a patch's code, and
the config.h layout, come out the way their authors wrote them in C instead of being re-invented
from the Rust.

## Scope guard

Write only inside `/home/jonas/Code2/C/dwmc` and `/home/jonas/Code2/C/dwmblocksc`, plus - only to
create them when absent - `~/.config/dwmc/config.h` and `~/.config/dwmblocksc/blocks.h` (see
"Config"). **Never edit** `Code2/Rust/dwmr`, `Code2/Rust/dwmblocksr`, `~/Downloads/dwm`,
`~/.config/dwm`, `~/.config/dwmblocks`, `~/.config/dwmr` or `~/.config/dwmblocksr`. If the Rust
looks wrong, port it faithfully and say so in the summary instead of fixing it on either side.
Never make backup copies of config files.

## Modes

Decide from the argument:

1. **Setup** - `setup`, or `dwmc/Makefile` does not exist yet. Scaffold dwmc (see "Setup mode").
   If the argument also named a file, continue into that file in the same run.
2. **Skeleton** - `skeleton <file>`. Mirror a Rust file's types, globals and function signatures
   in C with stub bodies (see "Skeleton phase").
3. **Implement** - `implement <file-or-function>`, or a bare Rust file / function name. Fill the
   stub bodies from the Rust (see "Implement phase"). Skeletonize first if needed.
4. **dwmblocksc** - `dwmblocksc`. Jump to the dwmblocksc group of the porting order, scaffolding
   that repo first if it is empty.
5. **Review** - `review <file-or-dir>`. Compare C against Rust and report gaps. Report only; do
   not modify files unless the user asks.
6. **Continue (no argument, or `continue`)** - work out what is missing and do the next slice.
   The default when the skill is invoked bare.

### Continue mode

**This mode is meant to be run over and over until both ports are finished.** Every run starts
from a cold context and re-derives its position from the trees, does one slice of work, leaves
both trees building, commits, and says what the next slice is. Nothing about a run may depend on
having seen the previous one.

#### 1. Orient

```bash
R=$code_root_dir/Code2/Rust
C=/home/jonas/Code2/C

ls "$C/dwmc/Makefile" "$C/dwmblocksc/Makefile" 2>&1
cat "$C/dwmc/PORT_STATUS.md" 2>/dev/null

# what still has stub bodies?
grep -rn 'TODO: port body' "$C/dwmc" "$C/dwmblocksc" --include='*.[ch]'

# which Rust files have no "Mirrors" line pointing at them yet?
grep -rho 'Mirrors [a-z]*/[a-z]*/[a-z_]*\.[a-z]*' "$C/dwmc" "$C/dwmblocksc" --include='*.[ch]' | sort -u
(cd "$R" && ls dwmr/src/*.rs dwmblocksr/src/*.rs)

# line counts so far
wc -l "$C"/dwmc/*.[ch] "$C"/dwmblocksc/*.[ch] 2>/dev/null
```

`PORT_STATUS.md` (in dwmc; it tracks both repos) is a hint that makes this cheaper, not a
substitute for it. Where the ledger and the tree disagree, the tree wins and the ledger is fixed.

#### 2. Pick the next slice

1. If dwmc is not set up, run setup mode first, then carry on into the porting order in the same
   run until the line target is met.
2. Otherwise walk the porting order and pick the first file that is missing, or that still has
   `TODO: port body` stubs. Skip "Not ported".
3. A finished file (present, no stubs) is not revisited, re-checked or tidied in a continue run -
   that is review mode's job. Touch it only if the current slice needs a change in it.

#### 3. Do the work

1. Port the picked file completely, then carry on into the next one in the order. **Aim for
   roughly 1000-2000 new lines of C per run for dwmc** - the target, not a ceiling to creep up on.
   A whole group in one pass is exactly right. dwmblocksc is small (about 1100 Rust lines, and
   `dwmblocks.c` is 435 lines of C), so it is done in **one run of its own**, whatever its size.
2. Stop when the run is about that size, when the order is exhausted, or when the build breaks in
   a way this run did not cause.
3. `dwm.rs` (about 4000 Rust lines, ~3000 lines of C) is bigger than one run. Skeletonize all of
   dwm.c first (includes, macros, enums, structs, the function declaration list, globals, the
   `#include "config.h"` and the compile-time checks after it, the handler table, stubs for every
   function), then implement it function by function in dwm's order over as many runs as it
   takes. A run that ends mid-file leaves every unported function as a stub with
   `/* TODO: port body - dwm.rs -> Dwm::<name> */` - the stubs *are* the resume pointer; the next
   run continues at the first one in file order.
4. Build before finishing (see "Verification"). A run always ends on building trees.

#### 4. Record, commit and report

Update `PORT_STATUS.md`, commit (see "Committing"), then summarize per "Output expectations",
ending with a single line:

```text
Next: implement dwm.c (Dwm::manage onwards)
```

#### 5. When to stop, and when it is done

- **Normal stop** - the line target is met. Report and stop; do not start another file to round
  the number up.
- **Blocked** - missing headers or libraries, a build broken by code this run did not touch, or
  the Rust itself looks wrong. Say what is blocking, leave the tree building if it was, and stop.
  Do not reshape the project to get around it.
- **Nothing left** - every Rust file outside "Not ported" has a C counterpart, `grep -rn 'TODO:
  port body'` comes back empty in both repos, the functional tests of "Verification" pass for both
  programs, and the user's config files exist (see "Config"). Say the port is complete and suggest
  `review dwmc` / `review dwmblocksc`. **Do not invent work**: no new features, no refactors. An
  empty run that says "nothing left to port" is a correct run.

Reruns are idempotent: scaffolding is created once, finished files are left alone.

### Porting order

Each group depends on the ones above it.

**dwmc** (`Code2/C/dwmc`), from `Code2/Rust/dwmr`:

1. **Setup** - Makefile, config.mk, LICENSE, README.md, .gitignore, PORT_STATUS.md, the logo
   (see "Setup mode").
2. **util** - `util.rs` -> `util.c`/`util.h`: `die()`, `ecalloc()`, `between()`,
   `truncate_utf8()`. The C-like `atoi`/`strtoul`/`strtof` helpers were for the TOML loader or
   are libc in C - drop them and say so.
3. **drw** - `drw.rs` (+ `fontconfig.rs`, which disappears: C has the headers) -> `drw.c`/`drw.h`,
   following dwm's drw.c plus what the status2d/alpha patches changed in it.
4. **transient** - `examples/transient.rs` -> `transient.c`.
5. **config** - the types of `config.rs` (`Arg`, `Key`, `Button`, `Rule`, `Layout`, `Command`
   as argv arrays, the scratchpad and xresources types) go into dwm.c where dwm declares its
   types; `Config::default()` -> `config.def.h`; dwmr's `config/config.toml` ->
   `config/config.h`. See "Config".
6. **dwm.c skeleton** - all of `dwm.rs` + `main.rs` + `xres.rs` (the latter disappears into
   `#include <X11/extensions/XRes.h>`) as stubs.
7. **dwm.c, first half** - `applyrules` .. `manage` in dwm's order.
8. **dwm.c, second half** - `manage` .. `zoom`, then `main()`.
9. **vanitygaps** - `vanitygaps.rs` -> `vanitygaps.c`, `#include`d by dwm.c like in
   `~/.config/dwm/dwm.c`.
10. **Man page, README, tests, functional test** - `dwmr.1` -> `dwmc.1` (with a CUSTOMIZATION
    section like dwm.1's, pointing at `~/.config/dwmc/config.h`), README complete, the ported unit
    tests, the Xvfb comparison against dwmr, and `~/.config/dwmc/config.h`.

**dwmblocksc** (`Code2/C/dwmblocksc`), from `Code2/Rust/dwmblocksr`, one run:

11. Setup (Makefile, config.mk, LICENSE, README.md, .gitignore), `dwmblocks.rs` + `main.rs` ->
    `dwmblocksc.c`, `Config::default()` -> `blocks.def.h`, dwmblocksr's `config/config.toml` ->
    `config/blocks.h`, `dwmblocksc.1`, the ported unit tests, the functional test against
    dwmblocksr, and `~/.config/dwmblocksc/blocks.h`.

### Not ported

- `target/`, `Cargo.toml`, `Cargo.lock` - build plumbing; `config.mk` + `Makefile` replace them.
- `src/fontconfig.rs`, `src/xres.rs` - FFI declarations; C includes the real headers.
- **The runtime config loaders** of dwmr and dwmblocksr: `config_path()`, `load()`, `parse()`,
  the `Raw*` TOML shapes, `parse_int_expr`, `parse_uint_expr`, `parse_mask`, `parse_keysym`,
  `parse_func`, `parse_arrange`, `parse_click`, `parse_arg`, `ConfigError`, and their unit tests.
  The headers are C, so the compiler does that work. What the loaders did besides parsing is
  kept: dwmr's xresources override, dwmblocksr's `selectblocks(hasbattery())` and delimiter
  cutting.
- The `// SAFETY:` comments - there are no `unsafe` blocks in C. Keep a comment only where the
  invariant it states is not obvious from the C.
- `.claude/` of the Rust repos.

Do not report any of these as missing.

## Project layout

```
dwmc/                           Code2/Rust/dwmr
  config.mk                     Cargo.toml [features]: XINERAMAFLAGS etc., like dwm's config.mk
  Makefile                      Makefile (all, test, debug, clean, install, install-config, uninstall)
  LICENSE                       LICENSE (dwm's MIT/X license, with a line for the port)
  README.md                     README.md, with dwmc.png at the top
  PORT_STATUS.md                ledger for both repos (committed in dwmc)
  dwmc.png, tools/logo.py       dwmr.png, tools/logo.py
  dwmc.1                        dwmr.1
  dwm.c                         src/dwm.rs + src/main.rs + the types of src/config.rs (+ xres.rs as an include)
  vanitygaps.c                  src/vanitygaps.rs (#included by dwm.c)
  config.def.h                  Config::default() in src/config.rs
  config/config.h               config/config.toml (the user's config, shipped)
  config.h                      generated by make, git-ignored (see "Config")
  drw.c, drw.h                  src/drw.rs (+ src/fontconfig.rs as header includes)
  util.c, util.h                src/util.rs
  transient.c                   examples/transient.rs
  test.c                        the #[cfg(test)] modules that are not about the TOML loader

dwmblocksc/                     Code2/Rust/dwmblocksr
  config.mk, Makefile, LICENSE, README.md, dwmblocksc.1
  dwmblocksc.c                  src/dwmblocks.rs + src/main.rs + the types and hasbattery/selectblocks of src/config.rs
  blocks.def.h                  Config::default() in src/config.rs
  config/blocks.h               config/config.toml (the user's blocks, shipped)
  blocks.h                      generated by make, git-ignored
  test.c                        the #[cfg(test)] modules that are not about the TOML loader
```

Every C file starts with dwm's `/* See LICENSE file for copyright and license details. */`
followed by `/* Mirrors dwmr/src/<file>.rs */` (or `dwmblocksr/src/...`, or
`.../config/config.toml` for `config/config.h` and `config/blocks.h`), so the correspondence stays greppable in both
directions. Functions keep the order they have in the Rust file (which is dwm's order).

## Setup mode

For dwmc, create in this order (dwmblocksc gets the same minus the logo, with `blocks.h`,
`blocks.def.h`, `config/blocks.h`, `~/.config/dwmblocksc` and `dwmblocksc` in place of the
`config.h` names, and dwmblocks' `-lX11`-only libraries):

1. **`config.mk`** - dwm's config.mk: `VERSION = 6.8.0` (dwmr's), `PREFIX`, `MANPREFIX`,
   `X11INC`/`X11LIB`, `XINERAMALIBS`/`XINERAMAFLAGS`, `FREETYPELIBS`/`FREETYPEINC`, `-lXrender`,
   `-lXRes`, and

   ```make
   CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700L -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
   CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} ${CPPFLAGS}
   LDFLAGS  = ${LIBS}
   ```

2. **`Makefile`** - dwm's Makefile shape plus dwmr's targets:
   - `config.h` is generated, never edited in the repo: it is a copy of the user's
     `~/.config/dwmc/config.h` when that exists, else of `config.def.h` (dwm's
     `cp config.def.h $@`, with the user's file taking precedence). It depends on the user's file,
     so editing `~/.config/dwmc/config.h` and running `make` rebuilds:

     ```make
     # under sudo, use the invoking user's config, not root's
     USERHOME = ${HOME}
     ifneq (${SUDO_USER},)
     USERHOME = $(shell getent passwd ${SUDO_USER} | cut -d: -f6)
     endif
     CONFDIR = ${USERHOME}/.config/dwmc

     config.h: config.def.h $(wildcard ${CONFDIR}/config.h)
     	if [ -e ${CONFDIR}/config.h ]; then cp ${CONFDIR}/config.h $@; else cp config.def.h $@; fi
     ```

     `dwm.o` depends on `config.h`, as in dwm's Makefile.
   - `all` builds `dwmc`; `test` builds and runs `test.c` with
     `-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer`; `debug` builds `dwmc-debug`
     with the same sanitizer flags for the functional test; `install` (binary + `dwmc.1` with
     `VERSION` substituted); `install-config` (copies `config/config.h` to
     `~/.config/dwmc/config.h` only if none exists); `uninstall`; `clean` (also removes the
     generated `config.h`).
3. **`.gitignore`** - `dwmc`, `dwmc-debug`, `test`, `*.o`, `transient`, `config.h` (the generated
   one at the top level only - `/config.h`, so `config/config.h` stays tracked).
4. **LICENSE**, **README.md** (from dwmr's README with dwmr -> dwmc, cargo -> make, the
   requirements as Debian packages: `libx11-dev libxft-dev libxinerama-dev libxres-dev
   libfontconfig-dev`, and the configuration section rewritten for `config.h`: edit
   `~/.config/dwmc/config.h`, `make && sudo make install`, restart dwmc).
5. **The logo** (see "Logo").
6. **`PORT_STATUS.md`**:

   ```markdown
   # dwmc / dwmblocksc port status

   Maintained by the `dwmc-port` skill. A hint for the next run, not the source of truth - the
   trees are. Re-derive with: `grep -rn 'TODO: port body' . ../dwmblocksc --include='*.[ch]'`

   **Last run:** setup - project scaffolding
   **Next:** util.c, drw.c

   ## Porting order

   - [ ] 1. setup
   - [ ] 2. util
   ...
   - [ ] 11. dwmblocksc

   ## Run log

   | Run | Files | ~C lines | Build | Tests |
   | --- | --- | --- | --- | --- |
   | setup | scaffolding | - | - | - |
   ```

   Tick a group only when it is ported, stub-free and verified. A half-done dwm.c gets a
   sub-bullet naming the last ported function.

Do not create empty files for code that has not been ported yet.

## Logo

dwmr has a custom logo, `dwmr.png`, drawn by `tools/logo.py` in the blocky style of dwm's
`dwm.png`. dwmc gets the same treatment:

- Copy `tools/logo.py` to `dwmc/tools/logo.py` and change it to draw "dwmc": the default output is
  `dwmc.png`, the docstring/description say dwmc, `RGAP_COLUMN`/`--rgap` become
  `CGAP_COLUMN`/`--cgap`, and the grid's last three columns are a `c` in the style of dwmr's `r`:

  ```python
  LOGO = [
      "...#............",
      "...#............",
      "####.#.#####.###",
      "#..#.#.#.#.#.#..",
      "########.#.#.###",
  ]
  ```

- Run it with the defaults (black on transparent, 16px cells, half-cell gap) to produce
  `dwmc.png`, check it with the Read tool that it reads as "dwmc", and put
  `<p align="center"><img src="dwmc.png" alt="dwmc"></p>` at the top of README.md, as dwmr does.
- dwmblocksr has no logo, so dwmblocksc gets none either.

## Config

### dwmc: config.h

dwmc is configured like dwm: `config.h` is `#include`d by dwm.c at the spot dwm includes it
("configuration, allows nested code to access above variables"), and everything in it is compiled
in.

- **`config.def.h`** is dwmr's `Config::default()` written back as dwm's config.def.h: dwm 6.8's
  config.def.h plus every option the ported patches add, with the patches' own variable names,
  default values and comments (take them from `~/.config/dwm/config.def.h` and the
  `~/.config/dwm/patches/*.diff` hunks, and check each value against `Config::default()` - the Rust
  wins where they differ). Commands are `static const char *termcmd[] = { "st", NULL };` arrays,
  key and button arrays use dwm's `TAGKEYS`/`SHCMD`/`MODKEY` macros, `.v = cmd` and
  `.v = &layouts[n]` as dwm writes them.
- **`config/config.h`** is dwmr's shipped `config/config.toml` written back as a config.h - which
  is where that TOML came from ("Ship my dwm config.h as config/config.toml"), so use
  `~/.config/dwm/config.h` as the layout template and `config/config.toml` as the truth for every
  value, including later changes the C config.h does not have. The `/* bind ... */` comments of
  the TOML go back above their key lines. Program names are swapped: `dwmr` -> `dwmc`,
  `dwmblocksr` -> `dwmblocksc` (the `statusbar` variable, the autostart command, the
  `kill -44 $(pidof ...)` volume commands, the command that opens the blocks config, now
  `~/.config/dwmblocksc/blocks.h`).
- **dwmr-only config conveniences disappear or become C**: integer-expression strings
  (`"1 << 8"`) are plain C expressions; keysym names are `XK_*` constants; `"MODKEY|ShiftMask"`
  strings are C masks; the `[commands]` table is argv arrays; the xresources patch's
  `ResourcePref resources[]` array goes back into config.h, and `load_xresources()` still
  overrides the compiled-in values at startup as dwmr does.
- **Compile-time checks** for every `parse()` validation that can be static, placed right after
  `#include "config.h"` in dwm.c with dwm's negative-array-size idiom and a comment naming the
  dwmr error it replaces. What depends on runtime state stays a runtime check at the point of use.
- **Unit test `shipped_config_is_config_h`** becomes: `make test` compiles `test.c` once against
  `config.def.h` and once against `config/config.h`, so both always build.

### dwmblocksc: blocks.h

dwmblocksc is configured like dwmblocks: `blocks.h` is `#include`d by dwmblocksc.c where
dwmblocks.c includes it, and generated by the Makefile exactly like dwmc's `config.h` (a copy of
`~/.config/dwmblocksc/blocks.h` if it exists, else of `blocks.def.h`; rebuilt when the user's file
changes; `make install-config` copies `config/blocks.h` there if absent).

- **`blocks.def.h`** is dwmblocksr's `Config::default()` written back in blocks.def.h's form: the
  `static const Block blocks[]` table (icon, command, interval, signal), `delim[]`/`delimLen`, and
  what dwmblocksr made config that was compile-time in C goes back to compile time: `ASYNC` is a
  `#define` again and the two `#if ASYNC` variants of dwmblocks.c come back as preprocessor
  branches (both are built and tested, see "Verification").
- **The battery swap stays a startup decision**, as in dwmblocksr (compile.sh's sb-battery ->
  sb-internet swap at build time is not brought back): `Block` gets a `battery` field with three
  values (always / only with a battery / only without), and `selectblocks(hasbattery())` picks the
  blocks at startup into a list allocated once and freed on exit.
- **`config/blocks.h`** is dwmblocksr's shipped `config/config.toml` written back as a blocks.h,
  using `~/.config/dwmblocks/blocks.h` as the layout template and the TOML as the truth for every
  value, with `dwmblocksr` -> `dwmblocksc` swapped. `\uXXXX` escapes in icons become the UTF-8
  characters (or `\x` escapes) as blocks.h writes them.
- **Compile-time checks** for dwmblocksr's `parse()` validations, after `#include "blocks.h"`, as
  for dwmc.
- **Unit test `shipped_config_matches_defaults`** becomes: `make test` compiles `test.c` against
  `blocks.def.h` and against `config/blocks.h`.

### The user's config files

In the final group of each program, when it runs end to end, create the user's copy if it does not
exist:

- `~/.config/dwmc/config.h` - `config/config.h` adjusted to the user's own
  `~/.config/dwmr/config.toml` (which may differ from the shipped one: carry every difference over,
  translated to C), with the name swaps above.
- `~/.config/dwmblocksc/blocks.h` - `config/blocks.h` adjusted to the user's own
  `~/.config/dwmblocksr/config.toml`, with the name swaps.

**Never overwrite an existing one and never make a backup copy**; if one exists, tell the user what
differs instead. These files are never part of a commit.

## Rust to C translation rules

The Rust is the source of truth for behaviour; the dwm/dwmblocks C is the source of truth for how it
looks. Preserve control flow, loops, conditions, arithmetic, call order and side effects, and keep
the comments in the same positions.

### Write C, not Rust in C syntax

**Keep the Rust's logic, write it the C way.** The port carries over *what* the Rust does - every
decision, check, fix and ordering - but the code itself follows C's (and specifically dwm's and
dwmblocks') structure, naming and idioms, not Rust's. Where the Rust shape exists only because of
Rust (ownership, borrowing, `Option`/`Result`, traits, iterators, enums with data, slab ids,
`Rc`), rewrite it the way a C programmer - and dwm's author - would, as long as the behaviour stays
the same. Concretely:

- **Naming**: C conventions and dwm's names - lower-case run-together function names as dwm has
  them (`focusstack`, `updatebarpos`), `snake_case` only where the name has no dwm counterpart,
  `UPPER_CASE` macros and enum constants (`ClkTagBar`, `NetWMName`, `SchemeNorm` keep dwm's
  CamelCase enum style), struct types `Client`/`Monitor`/`Drw` as in dwm. No `impl`-style
  `type_method` prefixes, no Rust type names (`Option`, `Vec`, `Result`, `Self`) in C
  identifiers.
- **Structure**: free `static` functions and file-scope globals, not a big state struct passed
  around; linked lists with `next` pointers, not index arrays; a function-pointer table, not a
  `switch` that mimics `match`; out-parameters or an `int` status, not tagged return structs;
  early `return`s and plain straight-line code, as dwm writes it.
- **Idioms**: `for`/`while` loops over pointers instead of iterator chains; `NULL` checks instead
  of `if let`; `!x` / `x == 0` for flags; `LENGTH()` on static arrays; `memset`/`memcpy`/
  `snprintf` for buffers; `ecalloc`/`free` with the owner clear from the code.
- **Formatting**: suckless style throughout (see "Naming and style"), and `/* */` comments.
- **When in doubt**, look at how `~/Downloads/dwm/dwm.c`, `~/.config/dwm/dwm.c` or
  `~/.config/dwmblocks/dwmblocks.c` write the same thing and match that; if none of them has it,
  write what dwm's author would write, not what the Rust says line by line. A reviewer reading a
  dwmc function next to dwm's should see dwm's style; reading it next to dwmr's should see the same
  logic.

### dwmr's representations, back to dwm's

| Rust (dwmr)                                              | C (dwmc) |
| -------------------------------------------------------- | -------- |
| `struct Dwm` fields (`selmon`, `mons`, `stext`, `bh`, atoms, ...) | file-scope `static` globals in dwm.c with dwm's names |
| `Vec<Client>` slab, `ClientId`, `Option<ClientId>`       | `Client *` linked lists with `next`/`snext`, `NULL` - as in dwm.c; `unmanage` frees the client |
| `Vec<Monitor>`, `MonId`, `mons.len() > 1`                | `Monitor *mons` list with `next`, `mons->next` |
| `Dwm::layouts: Vec<Layout>` + indices in `Monitor.lt`    | `const Layout *lt[2]` pointing into config.h's `layouts[]` (and dwm's `foo` empty layout in `cleanup()`) |
| `Rc<Config>`, `self.config.borderpx`, `config.keys`      | config.h's statics: `borderpx`, `keys[]`, `LENGTH(keys)` - no struct, no clone |
| `fn(&mut Dwm, &Arg)` (`KeyFn`), `fn(&mut Dwm, MonId)`    | `void (*func)(const Arg *)`, `void (*arrange)(Monitor *)` |
| `enum Arg` + `arg.i()`/`ui()`/`f()`/`is_zero()`          | dwm's `union Arg`, `arg->i` etc. |
| `Arg::V(Rc<Command>)`, `Arg::Layout(n)`                  | `.v = termcmd`, `.v = &layouts[n]` |
| `handler(ty)` match                                      | `static void (*handler[LASTEvent]) (XEvent *)` |
| `let ev: XButtonEvent = e.into()`                        | `XButtonEvent *ev = &e->xbutton;` |
| `String` + `truncate_utf8(s, N - 1)`                     | `char s[N]` + `truncate_utf8()` / `strncpy` at the same places |
| `Vec<Fnt>` (index 0 primary)                             | drw's `Fnt *fonts` list |
| `XERRORXLIB: OnceLock`                                   | `static int (*xerrorxlib)(Display *, XErrorEvent *)` |
| `WIDTH`/`HEIGHT`/`INTERSECT` fns, `textw(...)`           | dwm's macros `WIDTH`, `HEIGHT`, `INTERSECT`, `ISVISIBLE`, `CLEANMASK`, `TAGMASK`, `TEXTW` |
| constants the `x11` crate lacked (`XC_*`, request codes) | the real headers (`<X11/cursorfont.h>`, `<X11/Xproto.h>`) |
| `#[cfg(feature = "xinerama")]`                           | `#ifdef XINERAMA` |
| load-time validation in `parse()`                        | compile-time checks after `#include "config.h"` (see "Config") |
| `#[cfg(test)] mod tests`                                 | `test.c` (see "Verification") |
| `env!("CARGO_PKG_VERSION")`                              | `VERSION` from config.mk |
| `libc::signal(SIGPIPE, SIG_DFL)` in main                 | not needed - a C program starts with the default disposition; drop it with a comment |

### dwmblocksr's representations, back to dwmblocks'

| Rust (dwmblocksr)                                        | C (dwmblocksc) |
| -------------------------------------------------------- | -------------- |
| `struct Dwmblocks` fields                                | file-scope `static` globals with dwmblocks.c's names |
| `SIGPIPE_FD`, `STATUS_CONTINUE`, `SIGPLUS` atomics       | `static int` / `static volatile sig_atomic_t` as in dwmblocks.c |
| `BlockStatus { buf, len }`, `BlockCmd.out`               | `char [CMDLENGTH]` buffers, one per selected block, allocated once at startup |
| `config.async` + `getcmd_async`/`getcmd_sync`            | `#define ASYNC` in blocks.h and dwmblocks.c's `#if ASYNC` branches |
| `Config` / `Rc<Config>`                                  | blocks.h's statics: `blocks[]`, `delim`, `delimLen`, `LENGTH(blocks)` |
| `WriteStatus` enum + `writestatus()`                     | dwmblocks' `static void (*writestatus)(void)` = `setroot` / `pstdout` |
| `envp` built before `fork()` for `BLOCK_BUTTON`          | the same: build it before forking, `execve` in the child (not `setenv` after fork) |
| `battery` key + `selectblocks(hasbattery())`             | `Block.battery` + the same function names, in dwmblocksc.c |
| load-time validation in `parse()`                        | compile-time checks after `#include "blocks.h"` |
| `setupx` (lower-cased)                                   | `setupX`, dwmblocks' spelling - C names go back to the C spelling |

### Naming and style

- **Function names are dwm's/dwmblocks'**, identical in the Rust and the C (`applyrules`,
  `focusstack`, `getcmds`, `statusloop`). Rust-only helpers keep their Rust name (`truncate_utf8`).
- Globals, struct fields, config variables and constants keep dwm's C names and types (`int` flags
  where dwm has them, `unsigned int tags`, `char name[256]`), and the patches' names for what the
  patches add.
- suckless style: tabs, the return type on its own line above the function name, `/* */` comments
  only, declarations at the top of blocks where dwm puts them, function declarations listed at the
  top of dwm.c, `static` for everything file-local, no `typedef` beyond dwm's.
- Keep the Rust comments (they are mostly dwm's). Drop comments that only explain Rust mechanics
  (borrowing, slab ids, `Rc`) or the TOML loader.

## Skeleton phase

1. Read the Rust file end to end, and its C counterpart in `~/Downloads/dwm` / `~/.config/dwm` /
   `~/.config/dwmblocks` if there is one.
2. Write the C file with its includes, macros, enums, structs, globals, function declarations and a
   definition for every function whose body is only
   `/* TODO: port body - <file>.rs -> <Type>::<fn> */` plus a default return (`0`, `NULL`).
3. Leave the tree building (`make`; warnings about unused parameters in stubs are acceptable only
   while the stub exists).

## Implement phase

1. Read the Rust function end to end, and the C version in the references, before writing C.
2. Dependencies first: if it needs something not ported yet, port that too, fully.
3. Translate per the rules above; where the C reference and the Rust agree, the result should be
   that C reference's code (with the Rust's comments and fixes). Where they differ, follow the Rust
   and keep its comment. Where the Rust reads a config value, the C reads the config.h variable of
   the same name.
4. Remove the TODO once the body is real.
5. For every allocation written, name where it is freed. For every array index, name why it is in
   range. If the answer is "it isn't always", add the check the Rust implicitly had.

## Verification (every run, before committing)

```sh
cd /home/jonas/Code2/C/dwmc
make clean && make                              # gcc, must be warning-free
make clean && make CC=clang                     # clang, must be warning-free
make clean && make XINERAMAFLAGS= XINERAMALIBS= # the non-Xinerama variant must build
make test                                       # unit tests under ASan + UBSan, against config.def.h and config/config.h
scan-build-19 make                              # clang static analyzer: no reports in our files
```

When a run builds dwmc, check which `config.h` it used (the user's, if `~/.config/dwmc/config.h`
exists) and also build once with `config.def.h` (`make clean && make CONFDIR=/nonexistent`), so
both configs keep compiling. dwmblocksc: the same gcc/clang/`make test`/scan-build set, with
both configs, and built once with `ASYNC` 1 and once with 0.

Once dwm.c is fully ported (and for dwmblocksc in its run), the functional tests of the Rust
skills apply unchanged, with `make debug` (the sanitized binary) instead of the release one:

```sh
apt-get download xvfb && dpkg -x xvfb_*.deb xvfb     # in the scratchpad, no root needed
./xvfb/usr/bin/Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 &
DISPLAY=:99 ./dwmc-debug >dwmc.log 2>&1 &
DISPLAY=:99 xterm & DISPLAY=:99 xdotool key alt+Return   # drive it; check with xwininfo/xprop
```

- dwmc passes if `dwmc.log` holds no sanitizer report, the process stays alive through the driven
  session (spawn, focus, tag, layout switches, float, fullscreen, kill, scratchpad, swallow, clicks
  on the bar and status), and `xdotool key alt+shift+q` makes it exit 0 with no LeakSanitizer
  report from a dwmc frame. Leaks entirely inside libX11/libXft/libfontconfig are suppressed with
  `LSAN_OPTIONS=suppressions=tools/lsan.supp` - keep that file minimal and commented, and never
  suppress a frame of our own code.
- **Compare against dwmr**: run `dwmr` on `:98` with `config/config.toml` and `dwmc` on `:99` built
  with `config/config.h` (the same settings), drive both with the same scripted xdotool sequence,
  and check the client geometries (`xwininfo -root -tree`), the `_NET_*` properties
  (`xprop -root`) and a screenshot of the bar (`import -window root`) are identical.
- `dwmc` on the live display must print `dwmc: another window manager is already running` and exit
  1.
- dwmblocksc: dwmblocksr's recipe - root name read with a tiny `XFetchName` program and `od -c`,
  compared byte for byte against `dwmblocksr` with the same config in both `async` modes and with
  `-p`; update signals (`kill -$((34+n))`), clicks via `sigqueue` with `BLOCK_BUTTON`, SIGTERM
  exits 0 with an empty root name, no zombies, no sanitizer report. dwmblocksr runs with the
  TOML config and dwmblocksc with the blocks.h holding the same blocks.
- Background processes redirect stdout/stderr to files; kill by PID, never `pkill -f` a pattern
  that also appears in the invoking command line.

If a check cannot run (a missing package, no Xvfb), say so explicitly; do not report it as passed.

## Install and run

`make`, `sudo make install` (`/usr/local/bin/dwmc` and the man page; the Makefile picks up the
invoking user's `~/.config/dwmc/config.h` under sudo), `make install-config`. After editing
`~/.config/dwmc/config.h`: `make && sudo make install` and restart dwmc. `~/.xinitrc` can start it
with `WM=dwmc startx` once the user switches; do not edit `~/.xinitrc`. dwmblocksc the same way
with `~/.config/dwmblocksc/blocks.h`; dwmc's autostart starts it.

## Committing

After a run passes the checks above, commit in each repo it touched (unless told otherwise): a
short summary line prefixed with the project (`dwmc: port drw.c and util.c from dwmr`), a blank
line, then a body saying what was ported, what was adapted from the Rust and why, and what was
pulled in as a dependency. No `Co-Authored-By` line. The user's `~/.config/dwmc` and
`~/.config/dwmblocksc` files and the generated `config.h`/`blocks.h` are never part of a commit.
`PORT_STATUS.md`, `config/config.h` and `config/blocks.h` are.

## Review mode

Compare the requested scope and report:

1. Missing C files, functions, globals, config variables (every `Config` field of dwmr and
   dwmblocksr must have a config.h/blocks.h counterpart).
2. `TODO: port body` stubs still present.
3. Behaviour differences against the Rust (logic, messages, exit codes, config values in
   `config.def.h`/`blocks.def.h` vs `Config::default()` and `config/config.h`/`config/blocks.h`
   vs `config/config.toml`).
4. Safety gaps: unchecked indices, unchecked `NULL`, unbounded string writes, signed overflow,
   division by a configurable value, a `parse()` validation with no compile-time or runtime
   counterpart, non-async-signal-safe calls after `fork()` or in a handler.
5. Leaks: allocations with no free on some path.
6. Allocations in the event path.
7. Code that is longer than it needs to be, or helpers with no Rust/dwm counterpart.
8. Build warnings with gcc or clang, and scan-build reports.

Output: scope, matching parts, gaps, suggested next edits. Do not modify files unless asked.

## Output expectations

After a run, summarize:

- which mode ran, which files were picked and why, and roughly how many new lines of C
- which Rust files were skeletonized or implemented, and what was pulled in as a dependency
- which C files were added or updated, in which repo
- anything adapted rather than translated 1:1, and why (config values that moved from runtime
  validation to compile-time checks especially)
- which checks ran and their results (build with gcc/clang/no-Xinerama, both configs, `make test`,
  scan-build, functional tests), and which could not run
- that `PORT_STATUS.md` was updated and the commit(s) made
- a final `Next: <file or function>` line - always last, even when it is "nothing left to port, run
  `review dwmc` next"
