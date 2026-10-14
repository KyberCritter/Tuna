# GitHub Actions dependency pins

Release metadata and tag commit objects were queried from GitHub on **2026-10-14**.
These were the latest published stable releases at that check, not assumed major
versions. All referenced actions use complete commit SHAs in the workflow.

| Action | Verified release | Published (UTC) | Tag resolves directly to commit |
| --- | --- | --- | --- |
| actions/checkout | [v7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1) | 2026-07-20 | `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| msys2/setup-msys2 | [v2.33.0](https://github.com/msys2/setup-msys2/releases/tag/v2.33.0) | 2026-09-27 | `ec48f7c5447b3140e2b088413ae3a55687bccb6e` |

To update a pin, query `repos/<owner>/<repo>/releases/latest`, then
`repos/<owner>/<repo>/git/ref/tags/<tag>`. If the ref is an annotated tag, resolve
its tag object to the underlying commit before pinning. Record the release/date
and run CI. Do not replace pins with mutable `@vN` tags.

No Python/setup-python action is used. Host CI runs the same GNU Make targets on
Linux (GCC/Clang), macOS (Clang), and Windows (MSYS2 UCRT64 GCC). MSYS2 compiler
packages follow its signed package repositories; runner images/package versions
are not immutable action pins. This is a cross-platform regression lane, not a
bit-for-bit reproducible toolchain or console/hardware acceptance claim.
