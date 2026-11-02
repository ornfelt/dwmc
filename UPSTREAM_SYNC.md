# dwmc upstream sync ledger

Maintained by the `dwmc-upstream-sync` skill. A hint, not the source of truth - the code is.

Source: the user's dwm fork, `~/.config/dwm` `master` (<https://github.com/ornfelt/dwm>).
Specification: dwmr, `~/Code2/Rust/dwmr` - its `UPSTREAM_SYNC.md` decides each row first.
Baseline: `723a850` - Status bar font Propo, start dwmblocks with sb-start (2026-10-31), dwmr `ffeda55`, dwmc `6a21ee1`.
Everything up to and including the baseline was ported before this ledger existed.
High-water mark: `723a850` `Status bar font Propo, start dwmblocks with sb-start` (the baseline; no rows yet)
dwm HEAD when last checked: `723a850` `Status bar font Propo, start dwmblocks with sb-start` (2026-11-02)
Remaining: `0` (`pending` and `asked` rows; `0` of them wait on dwmr)

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

None yet.

## Flagged

None yet.

## Open questions

None yet.
