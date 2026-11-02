---
name: dwmc-upstream-sync
description: Port new commits from the user's dwm fork (~/.config/dwm, the C dwm that dwmr and dwmc derive from) into dwmc, one at a time and in order - but only those that are applicable and worth having, following the decision dwmr's sync already made for each commit (dwmr is dwmc's specification) and deciding alone only where dwmr skipped for a Rust-only reason. Every dwm commit after the baseline has a row in UPSTREAM_SYNC.md - pending until looked at - with its origin, kind, dwmr's commit, status, handling and the dwmc commit, and upstream_info.txt names the dwm commit dwmc is synced through. Ports each commit following the dwmc-port skill, verifies with make, commits each with the dwm/dwmr subject and dwm-commit/dwmr-commit trailers, followed by a ledger commit. Asks before anything that would make dwmc differ from dwmr.
disable-model-invocation: true
argument-hint: "[empty means every remaining commit within the run budget, or: setup, status, rebuild, <dwm-commit>, skip <dwm-commit> <reason>, replay <dwm-commit>]"
---

# Sync dwm commits into dwmc

dwmc (`~/Code2/C/dwmc`, branch `main`, remote `origin` = `https://github.com/ornfelt/dwmc`) is
the C port of dwmr, which is itself the Rust port of the user's patched dwm. The **source** of this
sync is that dwm, and dwmr is the **specification** dwmc follows:

| | |
| --- | --- |
| Source repo | `~/.config/dwm` (branch `master`, `https://github.com/ornfelt/dwm`) - the user's patched dwm, suckless dwm 6.8 plus patches and config |
| How it gets suckless fixes | its own `dwm-upstream-sync` skill lands suckless dwm commits there, with their upstream subject |
| Specification | dwmr, `~/Code2/Rust/dwmr`: its `UPSTREAM_SYNC.md` decides each dwm commit first, and its commit for that dwm commit is what dwmc ports |
| Read with | `git -C ~/.config/dwm ...` and `git -C ~/Code2/Rust/dwmr ...` - no remotes are added |

**Read before porting anything:**

- `.claude/skills/dwmc-port/SKILL.md` (this repo) - the authority on how dwmr becomes C: the
  priorities (never crash, never leak, no allocation in the event path, minimal suckless C,
  fidelity to dwmr), the file layout, config.def.h / config/config.h, the Rust-to-C translation
  rules, the scope guard, the verification and the commit rules.
- `~/Code2/Rust/dwmr/.claude/skills/dwmr-dev/SKILL.md` - how dwmr represents dwm, so a dwmr
  commit can be read back into dwm's C.
- `~/Code2/Rust/dwmr/.claude/skills/dwmr-upstream-sync/SKILL.md` - how dwmr decides whether a dwm
  commit belongs in the port (its section 4 and "dwmr's deliberate differences"), the ledger
  format, and the skip reasons this skill reads.

This skill only adds *which* dwm commits dwmc takes, in which order, and how they are tracked.

dwmc must behave exactly like dwmr (dwmc-port: "the Rust wins"). So dwmc **does not re-decide** a
dwm commit dwmr has decided: it ports what dwmr ported and skips what dwmr skipped - with one
exception, the commits dwmr skipped because Rust cannot have the problem or Cargo replaces the
build files; C can, and dwmc has dwm's `config.mk`/`Makefile`, so dwmc decides those itself (see
section 4). Every dwm commit still gets a row, so `UPSTREAM_SYNC.md` always shows what was ported
and why the rest was not.

**Scope guard** (dwmc-port's, plus the user's config copy): write only inside
`~/Code2/C/dwmc`, plus `~/.config/dwmc/config.h` under the rule in section 5. **Never edit**
`~/.config/dwm`, `~/Code2/Rust/dwmr` or `~/.config/dwmr`. If dwmr or dwm looks wrong, port it right
for dwmc when it is a crash or leak, otherwise faithfully, and flag it (see "Flagged").

## Arguments

- **none** - port the remaining dwm commits, oldest first, within the per-run budget below.
- `setup` - or the ledger does not exist yet: create `UPSTREAM_SYNC.md` with the baseline and a
  row for every dwm commit after it, filled per "Picking up the current state", update
  `upstream_info.txt`, commit both (see "Committing the ledger"), then continue as **none**.
- `status` - only report the baseline, the high-water mark, the remaining list (and which rows
  wait on dwmr), the open questions and the open "Flagged" entries. When the ledger disagrees with
  the tree, report what "Picking up the current state" would change. Write nothing.
- `rebuild` - redo the ledger per "Picking up the current state", keeping the notes, Special
  handling entries, flags and open questions of rows the rebuild confirms. A ledger-only run.
- `<dwm-commit>` - port just that dwm commit, even out of order (it still needs dwmr's decision).
- `skip <dwm-commit> <reason>` - record the commit as `skipped` (reason `declined`) without
  porting it, Handling `requested`, with the reason under "Special handling" in the user's words.
- `replay <dwm-commit>` - redo a commit that already has a row, because the earlier port was
  wrong or incomplete, or because dwmr's row changed since (a dwmr replay). See "Replay".

## 1. Make sure the trees are ready

```bash
cd ~/Code2/C/dwmc
git status --short             # must be clean before starting - otherwise stop and ask
git -C ~/.config/dwm log -1 --format='%h %cs %s' master    # the source's current HEAD
git -C ~/Code2/Rust/dwmr status --short                     # report if dirty; read only committed state
```

Only **committed** history counts, in both `~/.config/dwm` (local `master`) and dwmr (local
`main`). dwmr's ledger is read from its commit: `git -C ~/Code2/Rust/dwmr show
main:UPSTREAM_SYNC.md`. If dwmr has no ledger yet, every row here waits on dwmr: say so and
suggest running `/dwmr-upstream-sync` in `~/Code2/Rust/dwmr` first.

## 2. The baseline

The baseline is **`723a850`** (`723a850aea08d0460f3cb94c6ba5a67c7216ddc4`) - "Status bar font
Propo, start dwmblocks with sb-start" (2026-10-31), ported as dwmr `ffeda55` and dwmc `6a21ee1`.
Everything at or before it was mirrored into dwmc before this skill existed (the port from dwmr on
2026-09-25, then one dwmc commit per dwmr change) and never comes up again. It is the same
baseline as dwmr's, so the two ledgers have the same rows. The baseline is fixed and recorded in
the ledger header; `upstream_info.txt` started there but moves on with the high-water mark.

```bash
BASE=723a850aea08d0460f3cb94c6ba5a67c7216ddc4
git -C ~/.config/dwm log --reverse --date-order --format='%h %cs %p | %s' "$BASE..master"
```

If `$BASE` is ever not an ancestor of dwm's `master` (history rewritten there), stop and ask.

## 3. The ledger

`UPSTREAM_SYNC.md` in the repo root (tracked) is the progress file. Every run reads it first and
updates it last. It is a hint that makes orientation cheap, not the source of truth - the code is.
(`PORT_STATUS.md` belongs to the dwmc-port skill and is not touched by this one.)

Shape:

```markdown
# dwmc upstream sync ledger

Maintained by the `dwmc-upstream-sync` skill. A hint, not the source of truth - the code is.

Source: the user's dwm fork, `~/.config/dwm` `master` (<https://github.com/ornfelt/dwm>).
Specification: dwmr, `~/Code2/Rust/dwmr` - its `UPSTREAM_SYNC.md` decides each row first.
Baseline: `723a850` - Status bar font Propo, start dwmblocks with sb-start (2026-10-31), dwmr `ffeda55`, dwmc `6a21ee1`.
Everything up to and including the baseline was ported before this ledger existed.
High-water mark: `<sha>` `<subject>` (every row up to and including this one is decided)
dwm HEAD when last checked: `<sha>` `<subject>` (<date of the run>)
Remaining: `<n>` (`pending` and `asked` rows; `<m>` of them wait on dwmr)

Statuses: `pending` (not looked at yet, or waiting on dwmr), `ported` (the change carried over
1:1), `adapted` (ported, but reshaped - by dwmr, or for dwmc), `partial` (part ported, part
skipped - the notes say which), `skipped` (not ported - the notes start with the reason),
`asked` (waiting on an answer from the user, nothing written).

Origin: `own` (the user's own dwm commit), `suckless` (a suckless dwm commit landed in the fork by
`dwm-upstream-sync`), `merge` (a merge commit). Kind: `code`, `config`, `docs`, `build`,
`housekeeping`. Both are copied from dwmr's row.

Handling, measured against dwmr's row: `as-is` (dwmc does what dwmr did - ported the same change,
or skipped for the same reason), `needed` (dwmc differs from dwmr because C needs it: a fix dwmr
skipped as not applicable to Rust, a build change, a C-only safety detail), `requested` (the user
asked). Anything but `as-is` has an entry under Special handling.

Skip reasons: dwmr's (`housekeeping`, `already in dwmr`, `not applicable (rust)`,
`not applicable (build)`, `no counterpart`, `deviation`, `declined`, `merge, changes nothing`),
marked `(follows dwmr)` when taken over, plus `already in dwmc` for the
commits dwmc decided itself.

| # | Date | dwm commit | Origin | Kind | Subject | dwmr | Status | Handling | dwmc commit | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

## Special handling

<one bullet per row whose Handling is not `as-is`: the row and commit, `needed` or `requested`,
how dwmc differs from dwmr's handling of it, and why - the C reason, or the user's request in
their own words>

## Flagged

<one bullet per evident bug found while porting - in the dwm commit's change (for dwm) or in
dwmr's port of it (for dwmr) - see "Flagged">

## Open questions

<one bullet per `asked` row: the question put to the user and what it is waiting on>
```

Rules for the ledger:

- **One row per dwm commit after the baseline, every one of them**, in the same order and with
  the same `#` as dwmr's ledger (both append in dwm's `--date-order` order from the same
  baseline). Each run first adds `pending` rows for the commits that have none yet. Rows never
  move. `Date` is the dwm commit date.
- `dwmr` is dwmr's decision for the row, as `<status> <dwmr commit>` (`ported 1a2b3c4`,
  `skipped -`), or `pending` while dwmr's row is `pending`/`asked`. Refresh it every run.
- A `skipped` row's notes **start with the skip reason** from the list, then the specifics.
- `dwmc commit` is the short sha of the dwmc commit carrying the `dwm-commit:` trailer, or `-`.
- **Handling** as above; `requested` wins over `needed`.
- The **high-water mark** is the last row before the first row that is still `pending` or `asked`
  (a row waiting on dwmr counts as `pending`). A commit ported out of order does not move it.
- `Remaining` is the `pending` and `asked` rows, recomputed each run.
- The `Notes` column says which dwmc functions changed, what was translated differently from the
  dwmr commit and why, what went into `config.def.h`, `config/config.h` and the user's config,
  and `flagged` when the row has an entry under "Flagged".
- If the ledger disagrees with dwmc's `main`, **the tree wins** and the rows concerned are
  rebuilt per "Picking up the current state" in the same run.

### upstream_info.txt

`upstream_info.txt` in the repo root (tracked) names the dwm commit dwmc is synced through - the
high-water mark's full hash (the baseline while no row after it is decided):

```text
dwm
  commit <full hash of the high-water mark>
```

It changes whenever the high-water mark moves, and is committed with the ledger. It is never ahead
of dwmr's.

### Picking up the current state

For each dwm commit after the baseline, in ledger order:

1. **An earlier sync**: `git log --format='%h %s' --grep="^dwm-commit: <full sha>" main` finds
   the dwmc commit that carries it (fall back to the subject with the name swaps of section 5).
   Found: `ported`, `adapted` or `partial` as dwmr's row (or as the dwmc diff shows), naming it.
2. **dwmr's row**: copy origin, kind and the `dwmr` column from dwmr's ledger. A dwmr skip whose
   reason dwmc follows (section 4) becomes the same skip, `(follows dwmr)`.
3. **Already in dwmc**: for a dwmr skip dwmc decides itself, check dwmc's code by intent:
   `skipped (already in dwmc ...)` when it is there.
4. Otherwise `pending`.

Recompute the high-water mark, `Remaining` and `upstream_info.txt` from the rows. Say in the report
how many rows this added or changed.

## 4. Deciding, from dwmr's row

Read dwmr's row for the commit, then:

| dwmr's row | dwmc |
| --- | --- |
| `pending` or `asked` | **wait**: stop the run here, before this row (later rows wait too, to keep the order). Report that dwmr has to decide it first. |
| `ported`, `adapted`, `partial` | **port dwmr's commit** to C (section 5). Same status as dwmr; Handling `as-is` unless C needs more. A `partial` leaves out what dwmr left out. |
| `skipped (housekeeping)`, `(merge, changes nothing)`, `(no counterpart)`, `(deviation)`, `(declined)` | **skip, follows dwmr**, Handling `as-is`. For `no counterpart`, check that dwmc has no counterpart either (dwmc carries exactly dwmr's code, so it should not). |
| `skipped (already in dwmr)` | **check dwmc has it too**: yes - `skipped (already in dwmr) (follows dwmr)`; no - a parity gap: port the dwm change into dwmc, Handling `needed`, and say so in the report. |
| `skipped (not applicable (rust))` | **decide alone**: the bug may well exist in C. Check dwmc's code (it mirrors dwmr's logic, but in C: `free`, buffers, signed arithmetic, NULL). dwmc has the bug - port the dwm fix, which is C already and often fits dwmc's dwm.c almost verbatim, Handling `needed`. dwmc's code already rules it out (dwmc-port's rules: `snprintf`/`truncate_utf8`, unsigned wraparound, NULL tests, frees in `cleanup()`) - `skipped (already in dwmc ...)`, naming the code. |
| `skipped (not applicable (build))` | **decide alone**: dwmc has dwm's `config.mk`/`Makefile`. Take the change when it applies to dwmc's build (a flag, a library dwmc links, an install step), Handling `needed`; else `skipped (not applicable (build)) (follows dwmr)`. |

A version bump is ported by dwmr (`Cargo.toml`); dwmc takes it as `VERSION` in `config.mk` with
dwmr's three-part form (`6.8.0`), so `dwmc -v` prints what `dwmr -v` prints.

dwmc never ports a dwm change dwmr skipped for a behavioural reason, and never skips one dwmr
ported. If following dwmr would be wrong for dwmc - a C crash or leak in dwmr's logic, a C-only
problem - fix it in dwmc, record `needed`, and flag it for dwmr. If dwmc and dwmr would end up
behaving differently, **ask** (section 5, step 6).

### The file map

| dwm (`~/.config/dwm`) | dwmr (the spec) | dwmc | Kind |
| --- | --- | --- | --- |
| `dwm.c` | `src/dwm.rs`, `src/main.rs`, `src/xres.rs` | `dwm.c` | code |
| `vanitygaps.c` | `src/vanitygaps.rs` | `vanitygaps.c` | code |
| `drw.c`, `drw.h` | `src/drw.rs`, `src/fontconfig.rs` | `drw.c`, `drw.h` | code |
| `util.c`, `util.h` | `src/util.rs` | `util.c`, `util.h` | code |
| `transient.c` | `examples/transient.rs` | `transient.c` | code |
| `config.def.h` | `Config::default()` in `src/config.rs` | `config.def.h` (+ a compile-time check in dwm.c for each new `parse()` validation) | config |
| `config.h` | `config/config.toml` | `config/config.h`, and the user's `~/.config/dwmc/config.h` (section 5) | config |
| `dwm.1`, `README` | `dwmr.1`, `README.md` | `dwmc.1`, `README.md` | docs |
| `config.mk`, `Makefile` | `Cargo.toml`, `Makefile` | `config.mk`, `Makefile` | build |
| `.claude/`, `patches/`, `*.diff`, `gen_diffs.sh`, `UPSTREAM_SYNC.md`, `upstream_info.txt`, `dwm.png`, `.gitignore`, `LICENSE` | nothing | nothing | housekeeping |

Tests: a dwmr `#[cfg(test)]` change that is not about the TOML loader goes into `test.c`.

## 5. Port each commit, in order

The work list is the ledger's `pending` rows, oldest first, up to the first one that waits on
dwmr (for `<dwm-commit>`, just that one). If nothing is left, say dwmc is caught up (or caught up
with dwmr, naming what dwmr still has to decide) and stop.

For each commit in the work list:

1. **Decide** per section 4. A skip needs no dwmc commit: fill in the row, then move on (it is
   committed with the next ledger commit).
2. **Read both diffs**: dwmr's commit (`git -C ~/Code2/Rust/dwmr show <dwmr commit>`) is *what*
   to do - its logic, its deviations from the dwm diff, its config values, its docs and tests. The
   dwm commit (`git -C ~/.config/dwm show <sha>`) is *how it looks in C* - dwmc's dwm.c reads like
   dwm.c, so take dwm's hunks as the starting point and bring them to what dwmr does. Where they
   disagree, **dwmr wins** (dwmc-port: "the Rust wins"), and the deviation gets dwmr's comment.
3. **Port it** per the dwmc-port skill: functions in dwmr's (dwm's) order, dwm's naming and
   comment style, no allocation in the event path, every allocation freed in `cleanup()`, bounds
   and NULL checks where the Rust had them, `snprintf`/`truncate_utf8` for fixed buffers, unsigned
   arithmetic where the Rust wrapped. New config validations become compile-time checks after
   `#include "config.h"`.
4. **Config** - dwmr's `Config::default()` change goes into `config.def.h`; its
   `config/config.toml` change goes into `config/config.h` (layout from `~/.config/dwm/config.h`,
   values from the TOML, the `/* bind ... */` comments above their key lines), with the name
   swaps `dwmr` -> `dwmc`, `dwmblocksr` -> `dwmblocksc`, `~/.config/dwmblocksr/config.toml` ->
   `~/.config/dwmblocksc/blocks.h`. Then the **user's copy**, `~/.config/dwmc/config.h`: if it was
   identical to the shipped `config/config.h` before this change
   (`git show HEAD:config/config.h | diff - ~/.config/dwmc/config.h`), apply the same change to it
   so the two stay identical; otherwise leave it alone and say in the report exactly what to add.
   This standing rule is the user's request and overrides dwmc-port's "never overwrite an existing
   one". The user's copy is never part of a commit, and no backup copy is made.
5. **Docs and tests** - mirror dwmr's `dwmr.1` / `README.md` changes into `dwmc.1` / `README.md`
   (names swapped), and its non-loader unit tests into `test.c`.
6. **Ask first** - stop before committing when dwmc would have to behave differently from dwmr,
   when following dwmr looks wrong for C beyond a plain crash/leak fix, or when a decision dwmc
   makes alone (section 4) is not clear-cut. Explain what the commit does, what dwmr did, what
   would change in dwmc and why, with a recommended path, and wait. Leave the tree as it was
   (`git reset --hard HEAD && git clean -fd` after showing the planned change - the tree was clean
   at the start; restore the user's config copy if it was touched), record the row as `asked`
   with the question under "Open questions", and do not continue with later commits. Porting it
   after a "yes" is `requested`.
7. **Verify** (see below). Fix failures caused by this port; report pre-existing ones.
8. **Commit** - one dwmc commit per dwm commit, never squashed:

   ```bash
   git commit -m "<subject>" -m "<body>" -m "dwm-commit: <full dwm sha>
   dwmr-commit: <full dwmr sha>"
   ```

   - **Subject**: dwmr's commit subject with `dwmr` -> `dwmc` and `dwmblocksr` -> `dwmblocksc`,
     otherwise unchanged (e.g. dwmr `config: status bar font Propo, start dwmblocksr with
     sb-start` -> dwmc `config: status bar font Propo, start dwmblocksc with sb-start`). For a
     commit dwmc ports without a dwmr commit (a `needed` row), the dwm subject with the same swaps.
     No `dwmc:` prefix - that prefix belongs to the dwmc-port skill's porting commits.
   - **Body**: what the change does in dwmc (dwmr's body is the starting point, names swapped),
     what was translated differently from dwmr and why, and what was tested. Wrap at 72.
   - **Trailers**: `dwm-commit: <full dwm sha>`, then `dwmr-commit: <full dwmr sha>` when dwmr has
     a commit for it, as the last lines.
   - **No `Co-Authored-By`** or any other attribution line, on this commit or the ledger commit.
   - The ledger is **not** part of this commit: edit `UPSTREAM_SYNC.md` and `upstream_info.txt`
     only after it exists. `PORT_STATUS.md` is not touched.
9. **Record** the row, its Special handling / Flagged entries, add pending rows for new dwm
   commits, move the high-water mark, update `upstream_info.txt`, and commit the ledger:

   ```bash
   git add UPSTREAM_SYNC.md upstream_info.txt && git commit -m "update upstream sync ledger"
   ```

Then move on to the next commit.

### Flagged

While porting, check the dwm commit's own change and dwmr's port of it for an evident bug: a
crash, an out-of-bounds access, a leak, a wrong condition, a value used before it is set, behaviour
that contradicts the commit message, or dwmr doing something different from the dwm commit without
a comment saying it is deliberate. Style is not a bug. dwmc gets the commit without the crash or
leak (Handling `needed`); a behavioural difference is ported as dwmr has it. Add an entry under
"Flagged": the row and commit, where the bug is (dwm or dwmr, file and function), what goes wrong,
a repro, the suggested fix, and the decision, `open` until the user decides. Put `flagged` in the
row's notes. Never edit dwm or dwmr.

### Committing the ledger

The ledger and `upstream_info.txt` are committed in the `update upstream sync ledger` commit after
each ported commit. A run that changes them without porting a commit - skips, `asked`, `skip`,
`setup`, `rebuild`, refreshing the `dwmr` column - commits them the same way. When such decisions
come before a ported commit in the same run, fold them into that commit's ledger commit instead.
No attribution trailer.

### Replay

`replay <dwm-commit>` never rewrites a commit already in `main`. For a ported commit, the
correction - the rest of a `partial` port, a wrong translation, catching up with a dwmr replay
(a dwmr follow-up commit carrying the same `dwm-commit:` trailer) - is a new dwmc commit on top,
named after the dwmr follow-up when there is one, or for what it does
(`<function>: <what was fixed> (follow-up to dwm <short sha>)`), with the same trailers (the
`dwmr-commit:` of the follow-up), verified, followed by the ledger commit. The row is updated in
place. For a commit not ported yet (`asked`, `skipped`), replay ports it per this section.

To find dwmr follow-ups dwmc has not caught up with:
`git -C ~/Code2/Rust/dwmr log --format='%h %s' --grep='^dwm-commit: ' main` lists every dwmr
sync commit; one whose sha is not in any dwmc `dwmr-commit:` trailer is missing here.

## How much per run

One dwm commit at a time - one dwmc commit (plus its ledger commit) per ported dwm commit - until
roughly **500 changed lines** of dwmr diff have been ported in the run (`git -C ~/Code2/Rust/dwmr
show --numstat <dwmr commit>`, not counting `UPSTREAM_SYNC.md`), or stop earlier at a logical
point: a question for the user, a row that waits on dwmr, a large commit better done in its own
run, or a group of related commits just completed. Do not stop in the middle of such a group
because the budget is reached. Skips cost nothing against the budget.

Do not push. The user pushes. Do not run `sudo make install`; that is the user's call.

## Verification

dwmc-port's "Verification", every time code or config changed:

```sh
make clean && make                              # gcc, warning-free (but for the known ones PORT_STATUS.md lists)
make clean && make CC=clang                     # clang, warning-free
make clean && make XINERAMAFLAGS= XINERAMALIBS= # the non-Xinerama variant
make clean && make CONFDIR=/nonexistent         # built with config.def.h
make test                                       # ASan + UBSan, against config.def.h and config/config.h
scan-build-19 make                              # no reports in our files
```

`dwmc`, `config.h` and the `*.o` in the tree may be owned by root from `sudo make install`;
`make clean` can still remove them (the directory is the user's). If anything in the tree is in
the way, build a throwaway copy instead:
`TMP=$(mktemp -d) && git ls-files -z | xargs -0 cp --parents -t "$TMP" && make -C "$TMP" ...`,
then `rm -rf "$TMP"`.

For a `code` commit that changes behaviour, also run dwmc-port's functional test (`make debug`
in a local Xvfb, no sanitizer report, exits 0 on alt+shift+q) and exercise what the commit changed;
when the change is observable (bar, `_NET_*` properties, geometry), compare against dwmr built from
the dwmr commit, as dwmc-port describes. A config-only commit needs `make test` passing with the
new `config/config.h`. If a check cannot run, say so; never report it as passed.

## Report

At the end, list per dwm commit: sha, date, origin, kind, subject, dwmr's decision, dwmc's status
and handling with the skip reason or what was translated differently, or the question or dwmr row
it stopped on. Give any "Flagged" entry with its repro and suggested fix, any special handling with
its reason, and what was done to `~/.config/dwmc/config.h` (updated, or what the user must add).
Say how many `pending` rows were added and which rows a rebuild changed. Include the checks run
and their results, the dwmc commits created, the new high-water mark and `upstream_info.txt`, how
many rows remain and how many of those wait on dwmr.
