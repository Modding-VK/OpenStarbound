# GitHub Actions build flow

The Windows workflow builds four downloadable artifacts:

- the raw development `dist` tree;
- a client distribution;
- a server distribution;
- the Inno Setup installer.

## CI packaging

GitHub Actions already archives directories uploaded by `upload-artifact`.
The CI path therefore calls `assemble.bat --skip-archives` and uploads the
three output directories with compression level 1. The script still creates
the original high-compression client and server ZIP files when invoked without
that option, preserving its local behavior.

This removes three redundant level-9 7-Zip operations from CI. Runs before the
change spent roughly 9–11 minutes creating the client/server and full `dist`
ZIP files. Inno Setup continues to use its release compression, so the Windows
installer format and contents are unchanged.

## Compilation cache

The Windows `sccache` limit is 2 GB. The previous 1 GB limit produced large
variations in the combined CMake, vcpkg, build, and test step as cache entries
were evicted. The action publishes cache statistics in the job summary.

## Superseded runs

Concurrency is scoped to the workflow and Git ref. A newer run for the same
branch cancels an older in-progress run, avoiding work and artifacts for a
commit that has already been superseded. Runs for other branches are unaffected.
