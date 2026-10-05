# Contributing

Platinum sits in a five-repository stack: Wolfram (the C/C++ AT Protocol SDK)
underneath, then Metalbear (PDS), Cobalt (Wii U), Indigo (3DS) and this one. Shared
protocol logic belongs in Wolfram. Changes land there first and this repository
adopts them afterwards. [AGENTS.md](AGENTS.md) has the engineering rules; this
file is the flow.

## The flow

1. Open or find an issue. A gap against Cobalt or Indigo is a row in
   [docs/PARITY.md](docs/PARITY.md) (edit `docs/parity.tsv`, then run
   `tools/check-parity.sh --write`).
2. Branch from `main`. Never commit to `main`.
3. Write small commits with conventional subjects: `feat(macos9): ...`,
   `fix(bridge): ...`, `docs: ...`, `ci: ...`.
4. Open a pull request using the template. The title is a conventional commit
   too, because it becomes the squash subject.
5. Wait for the `CI gate` check to go green. If it is red, read the job log,
   reproduce it locally, fix the cause and push again. Skipping or deleting a
   test is not a fix.
6. Merge only a green pull request, by **rebase**. Every commit lands on `main`
   as written, so each one is a conventional commit that builds and passes on
   its own; write review fixes as real `fix(scope): ...` commits. Do not merge
   `main` into a branch and do not force-push. If a branch falls behind or
   conflicts, cut a fresh branch from `main`, cherry-pick, open a new PR that
   links the old one, and close the old one. A red `main` is fixed before anything else.

`tools/check-flow.sh` is what the `Flow and drift` job runs, so you can run the
same checks before pushing:

```sh
tools/check-flow.sh drift
tools/check-flow.sh commits origin/main..HEAD
```

## Releases

Tag `vX.Y.Z` only after bumping `bridge/package.json` and adding a section to
[CHANGELOG.md](CHANGELOG.md). The `release-check` workflow rejects a tag that
disagrees with either. It publishes nothing.

## What is enforced

| Rule | Enforced by |
| --- | --- |
| Conventional PR title and commits; no merge commits in a PR | `Flow and drift` job |
| Rebase merging only | repository setting; see the `needs-owner` issue |
| PR body has What / Why / Verification | `Flow and drift` job |
| Every source has an owner in `tools/ownership.txt`; no AT Protocol strings on the Mac side | `Flow and drift` job |
| Parity matrix matches the code, and cited issues are open | `Flow and drift` job |
| Every Mac source is compiled in CI; docs match the pinned Wolfram | `Flow and drift` job |
| Bridge type check, tests, build; Mac C89 compile and tests | `bridge`, `Mac OS 9 sources (C89)` |
| Tag matches version and changelog | `release-check` workflow |
| Nothing merges unless the above pass | `CI gate` job, which branch protection should require |

Branch protection on `main` is a repository setting that these workflows cannot
set. Until the owner requires `CI gate` there, the rule is by convention: see
the `needs-owner` issue.
