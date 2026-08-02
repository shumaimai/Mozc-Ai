#!/usr/bin/env python3
# Copyright 2024 AI Mozc IME Project
# Patch Mozc Windows installer to bundle AI setup files.

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def copy_installer_assets(ai_mozc_dir: Path, mozc_src: Path, dry_run: bool) -> None:
    src_dir = ai_mozc_dir / "installer" / "windows"
    dst_dir = mozc_src / "data" / "installer"
    for name in ("ai_config.default.json", "setup_ai_mozc.ps1"):
        src = src_dir / name
        dst = dst_dir / name
        print(f"copy {src} -> {dst}")
        if not dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)


def patch_data_installer_build(mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "data" / "installer" / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")
    marker = 'exports_files(["credits_en.html"])'
    replacement = (
        'exports_files([\n'
        '    "credits_en.html",\n'
        '    "ai_config.default.json",\n'
        '    "setup_ai_mozc.ps1",\n'
        '])'
    )
    if "ai_config.default.json" in text:
        print("data/installer/BUILD.bazel already patched; skipping")
        return
    if marker not in text:
        raise RuntimeError("Could not patch data/installer/BUILD.bazel")
    text = text.replace(marker, replacement, 1)
    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text, encoding="utf-8")


def patch_installer_build(mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "win32" / "installer" / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")

    if "ai_config.default.json" in text and 'MozcAI64.msi' in text:
        print("win32/installer/BUILD.bazel already patched; skipping")
        return

    if "ai_config.default.json" not in text:
        src_marker = '        "//data/installer:credits_en.html",'
        src_replacement = (
            '        "//data/installer:ai_config.default.json",\n'
            '        "//data/installer:credits_en.html",\n'
            '        "//data/installer:setup_ai_mozc.ps1",'
        )
        if src_marker not in text:
            raise RuntimeError("Could not find installer srcs marker")
        text = text.replace(src_marker, src_replacement, 1)

    if '_MSI_FILE = "MozcAI64.msi"' not in text:
        text = text.replace(
            '_MSI_FILE = "Mozc64.msi" if BRANDING == "Mozc" else "GoogleJapaneseInput64.msi"',
            '_MSI_FILE = "MozcAI64.msi" if BRANDING == "Mozc" else "GoogleJapaneseInput64.msi"',
            1,
        )

    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text, encoding="utf-8")


def patch_installer_wxs(mozc_src: Path, dry_run: bool) -> None:
    wxs_file = mozc_src / "win32" / "installer" / "installer_oss_64bit.wxs"
    text = wxs_file.read_text(encoding="utf-8")

    if 'Component Id="AIConfigDefault"' in text:
        print("installer_oss_64bit.wxs already patched; skipping")
        return

    feature_marker = '      <ComponentRef Id="CreditsEn" />'
    feature_replacement = (
        '      <ComponentRef Id="CreditsEn" />\n'
        '      <ComponentRef Id="AIConfigDefault" />\n'
        '      <ComponentRef Id="AISetupScript" />'
    )
    if feature_marker not in text:
        raise RuntimeError("Could not find Feature ComponentRef marker")
    text = text.replace(feature_marker, feature_replacement, 1)

    component_marker = """          <Component Id="CreditsEn">
            <File Id="credits_en.html" Name="credits_en.html" DiskId="1" Checksum="yes" Vital="yes" Source="$(var.DocumentsDir)/credits_en.html" />
          </Component>"""
    component_replacement = component_marker + """
          <Component Id="AIConfigDefault">
            <File Id="ai_config.default.json" Name="ai_config.default.json" DiskId="1" Checksum="yes" Vital="yes" Source="$(var.DocumentsDir)/ai_config.default.json" />
          </Component>
          <Component Id="AISetupScript">
            <File Id="setup_ai_mozc.ps1" Name="setup_ai_mozc.ps1" DiskId="1" Checksum="yes" Vital="yes" Source="$(var.DocumentsDir)/setup_ai_mozc.ps1" KeyPath="yes" />
          </Component>"""
    if component_marker not in text:
        raise RuntimeError("Could not find CreditsEn component marker")
    text = text.replace(component_marker, component_replacement, 1)

    sequence_marker = (
        '      <Custom Action="FixupConfigFilePermission" Before="EnableTipProfile" '
        'Condition="NOT (REMOVE=&quot;ALL&quot;)" />'
    )
    sequence_replacement = sequence_marker + """
      <Custom Action="SeedAIConfig" After="FixupConfigFilePermission" Condition="(NOT (REMOVE=&quot;ALL&quot;)) AND (NOT UPGRADING)" />"""
    if sequence_marker not in text:
        raise RuntimeError("Could not find InstallExecuteSequence marker")
    text = text.replace(sequence_marker, sequence_replacement, 1)

    custom_action_marker = (
        '    <CustomAction Id="EnableTipProfile" DllEntry="EnableTipProfile" '
        'Execute="commit" Impersonate="yes" BinaryRef="mozc_installer_helper.dll" />'
    )
    custom_action_replacement = custom_action_marker + """
    <CustomAction Id="SeedAIConfig"
                  Directory="MozcDir"
                  ExeCommand="powershell.exe -NoProfile -ExecutionPolicy Bypass -File &quot;[MozcDir]documents\setup_ai_mozc.ps1&quot; -Quiet"
                  Execute="deferred"
                  Impersonate="yes"
                  Return="check" />"""
    if custom_action_marker not in text:
        raise RuntimeError("Could not find custom action marker")
    text = text.replace(custom_action_marker, custom_action_replacement, 1)

    print(f"patch {wxs_file}")
    if not dry_run:
        wxs_file.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Patch Mozc Windows installer for AI Mozc")
    parser.add_argument("--mozc-dir", required=True, help="Path to mozc/src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    ai_mozc_dir = Path(__file__).resolve().parents[1]
    mozc_src = Path(args.mozc_dir).resolve()

    if not (mozc_src / "win32" / "installer" / "installer_oss_64bit.wxs").exists():
        print("error: Mozc Windows installer files not found", file=sys.stderr)
        return 1

    print(f"AI Mozc: {ai_mozc_dir}")
    print(f"Mozc src: {mozc_src}")
    print(f"Dry run: {args.dry_run}")
    print()

    copy_installer_assets(ai_mozc_dir, mozc_src, args.dry_run)
    patch_data_installer_build(mozc_src, args.dry_run)
    patch_installer_build(mozc_src, args.dry_run)
    patch_installer_wxs(mozc_src, args.dry_run)

    print()
    print("Installer integration complete.")
    print("Build MSI on Windows with:")
    print(f"  cd {mozc_src}")
    print("  python build_tools/update_deps.py")
    print("  python build_tools/build_qt.py --release --confirm_license")
    print("  bazelisk build package --config release_build")
    print("  # Output: bazel-bin/win32/installer/MozcAI64.msi")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
