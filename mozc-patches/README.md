# Mozc 「に」接続詞ビルド

This build starts from upstream `google/mozc` and adds exactly one manual dictionary entry:

```text
に    に    接続詞
```

Mozc's current OSS POS table maps the generic conjunction POS to ID 2593.
The GitHub Actions build verifies that the generated auxiliary dictionary contains `に` with left/right POS IDs 2593 before building the Windows installer.

Artifacts:

- `Mozc64-NiConjunction.msi`: the normal Mozc Windows MSI package.
- `MozcNiConjunctionSetup.exe`: a self-contained x64 bootstrapper that embeds the MSI and launches Windows Installer with elevation.

The EXE is not code-signed, so Windows SmartScreen may warn about an unknown publisher.
