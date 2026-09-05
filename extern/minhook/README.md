# MinHook (vendored, source form) - MODIFIED

Vendored copy of MinHook, compiled into `AmnesiaHeadTracking.asi` for function
hooking.

- Upstream: https://github.com/TsudaKageyu/minhook
- Copyright (C) 2009-2017 Tsuda Kageyu
- License: BSD-2-Clause - full text in `LICENSE.txt`, retained verbatim, and in
  every source file's header
- Base: upstream `master` (post-`v1.3.3`), source commit
  `1e9ad1eb42db11bfcb65461f687c656612d1b555`

Includes the **Hacker Disassembler Engine 32/64** under `src/hde/`,
Copyright (c) 2008-2009 Vyacheslav Patkov, also BSD-2-Clause. Separate work,
separate copyright holder; see `LICENSE.txt` and `THIRD-PARTY-NOTICES.md`.

## Local modifications

BSD-2-Clause permits modification. These are recorded so nobody mistakes this
for a stock upstream copy, and so behaviour that differs from upstream is not
reported to Tsuda Kageyu as an upstream bug.

- `src/hook.c` - allocate from `GetProcessHeap()` instead of `HeapCreate()`,
  and skip the matching `HeapDestroy()`, so the hook engine does not hold a
  private heap inside a host process it shares.
- `src/hook.c`, `src/buffer.c` - `static` dropped from the file-scope globals
  `g_isLocked`, `g_hHeap`, the hook-entry struct and `g_pMemoryBlocks`.
- `src/hook.c`, `src/trampoline.c` - relative-branch operands cast through the
  unsigned `UINT8`/`UINT32` rather than `INT8`/`INT32`.

Reproduce the diff against upstream at any time with:

    curl -sL https://raw.githubusercontent.com/TsudaKageyu/minhook/master/src/hook.c | diff - src/hook.c
