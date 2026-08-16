#!/usr/bin/env python3
"""Build the Mozc AI v1 all-in-one Windows installer inputs."""

from __future__ import annotations

import argparse
import hashlib
import html
import shutil
import sys
from pathlib import Path

PRODUCT_VERSION = "1.0.1"
MSI_FILE = f"MozcAI-{PRODUCT_VERSION}-x64.msi"
PRODUCT_NAME = "Mozc AI"
MANUFACTURER = "Mozc AI Project"
UPGRADE_CODE = "2917DE59-7EFA-46A3-B16A-1EE0BBEADBA4"
LEGACY_MOZC_UPGRADE_CODE = "DD94B570-B5E2-4100-9D42-61930C611D8A"


def stable_id(prefix: str, value: str) -> str:
    return prefix + hashlib.sha256(value.encode("utf-8")).hexdigest()[:20]


def copy_runtime(project: Path, mozc_src: Path, dry_run: bool) -> Path:
    source = (project / "runtime" / "bundle").resolve()
    if not (source / "rerank_daemon.exe").exists():
        raise RuntimeError(
            "runtime bundle is missing; run scripts/build_runtime_bundle.ps1 first"
        )
    destination = (mozc_src / "data" / "installer" / "ai_runtime").resolve()
    installer_root = (mozc_src / "data" / "installer").resolve()
    if installer_root not in destination.parents:
        raise RuntimeError(f"unsafe runtime destination: {destination}")
    print(f"copy runtime {source} -> {destination}")
    if not dry_run:
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination)
    return source if dry_run else destination


def directory_xml(root: Path, relative: Path, component_ids: list[str]) -> str:
    indent = "        " + "  " * len(relative.parts)
    lines: list[str] = []
    for file_path in sorted((root / relative).glob("*"), key=lambda p: p.name.lower()):
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(root).as_posix()
        component_id = stable_id("AICmp_", rel)
        file_id = stable_id("AIFile_", rel)
        component_ids.append(component_id)
        source = html.escape(str(file_path.resolve()), quote=True)
        name = html.escape(file_path.name, quote=True)
        lines.extend(
            (
                f'{indent}<Component Id="{component_id}" Guid="*">',
                f'{indent}  <File Id="{file_id}" Name="{name}" '
                f'DiskId="1" Checksum="yes" Vital="yes" Source="{source}" />',
                f"{indent}</Component>",
            )
        )
    for child in sorted(
        (p for p in (root / relative).iterdir() if p.is_dir()),
        key=lambda p: p.name.lower(),
    ):
        child_rel = child.relative_to(root)
        directory_id = stable_id("AIDir_", child_rel.as_posix())
        name = html.escape(child.name, quote=True)
        lines.append(f'{indent}<Directory Id="{directory_id}" Name="{name}">')
        lines.append(directory_xml(root, child_rel, component_ids))
        lines.append(f"{indent}</Directory>")
    return "\n".join(line for line in lines if line)


def generate_runtime_fragment(
    runtime_dir: Path, mozc_src: Path, dry_run: bool
) -> Path:
    fragment = mozc_src / "data" / "installer" / "ai_runtime.wxs"
    component_ids: list[str] = []
    body = directory_xml(runtime_dir, Path(), component_ids)
    refs = "\n".join(
        f'      <ComponentRef Id="{component_id}" />'
        for component_id in component_ids
    )
    xml = f"""<?xml version="1.0" encoding="utf-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Fragment>
    <DirectoryRef Id="MozcDir">
      <Directory Id="AIRuntimeRoot" Name="ai">
{body}
      </Directory>
    </DirectoryRef>
  </Fragment>
  <Fragment>
    <ComponentGroup Id="AIRuntimeComponents">
{refs}
    </ComponentGroup>
  </Fragment>
</Wix>
"""
    print(f"generate {fragment} ({len(component_ids)} runtime files)")
    if not dry_run:
        fragment.write_text(xml, encoding="utf-8")
    return fragment


def patch_data_installer_build(mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "data" / "installer" / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")
    if "ai_runtime_files" not in text:
        text = text.rstrip() + """

exports_files(["ai_runtime.wxs"])

filegroup(
    name = "ai_runtime_files",
    srcs = glob(["ai_runtime/**"]),
    visibility = ["//visibility:public"],
)
"""
    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text, encoding="utf-8")


def patch_installer_build(mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "win32" / "installer" / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")
    for previous in ("Mozc64.msi", "MozcAI-1.0.0-x64.msi"):
        text = text.replace(
            f'_MSI_FILE = "{previous}" if BRANDING == "Mozc" else "GoogleJapaneseInput64.msi"',
            f'_MSI_FILE = "{MSI_FILE}" if BRANDING == "Mozc" else "GoogleJapaneseInput64.msi"',
        )
    if "//data/installer:ai_runtime.wxs" not in text:
        marker = '        "//data/installer:credits_en.html",'
        addition = (
            '        "//data/installer:ai_runtime.wxs",\n'
            '        "//data/installer:ai_runtime_files",\n'
        )
        if marker not in text:
            raise RuntimeError("Could not find installer srcs marker")
        text = text.replace(marker, addition + marker, 1)
    if "--runtime_fragment=" not in text:
        marker = '"--credit_file=$(location //data/installer:credits_en.html)",'
        addition = (
            marker
            + '\n        "--runtime_fragment=$(location '
            + '//data/installer:ai_runtime.wxs)",'
        )
        if marker not in text:
            raise RuntimeError("Could not find installer command marker")
        text = text.replace(marker, addition, 1)
    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text, encoding="utf-8")


def patch_build_installer_py(mozc_src: Path, dry_run: bool) -> None:
    source = mozc_src / "win32" / "installer" / "build_installer.py"
    text = source.read_text(encoding="utf-8")
    if "args.runtime_fragment" not in text:
        marker = "  if args.mozc_tip64arm and args.mozc_tip64x:\n"
        addition = (
            "  if args.runtime_fragment:\n"
            "    commands += ['-src', args.runtime_fragment]\n"
            + marker
        )
        if marker not in text:
            raise RuntimeError("Could not find WiX command extension point")
        text = text.replace(marker, addition, 1)
    if "parser.add_argument('--runtime_fragment'" not in text:
        marker = "  parser.add_argument('--credit_file', type=str)\n"
        if marker not in text:
            raise RuntimeError("Could not find build_installer parser marker")
        text = text.replace(
            marker,
            marker + "  parser.add_argument('--runtime_fragment', type=str)\n",
            1,
        )
    print(f"patch {source}")
    if not dry_run:
        source.write_text(text, encoding="utf-8")


def patch_installer_wxs(mozc_src: Path, dry_run: bool) -> None:
    source = mozc_src / "win32" / "installer" / "installer_oss_64bit.wxs"
    text = source.read_text(encoding="utf-8")
    package_old = (
        '<Package Name="Mozc" Language="1041" Codepage="932" '
        'Version="$(var.MozcVersion)" Manufacturer="Google LLC" '
        'UpgradeCode="$(var.UpgradeCode)" InstallerVersion="500">'
    )
    package_new = (
        f'<Package Name="{PRODUCT_NAME}" Language="1041" Codepage="932" '
        f'Version="{PRODUCT_VERSION}" Manufacturer="{MANUFACTURER}" '
        f'UpgradeCode="{UPGRADE_CODE}" InstallerVersion="500">'
    )
    package_v1_0_0 = (
        f'<Package Name="{PRODUCT_NAME}" Language="1041" Codepage="932" '
        f'Version="1.0.0" Manufacturer="{MANUFACTURER}" '
        f'UpgradeCode="{UPGRADE_CODE}" InstallerVersion="500">'
    )
    if package_old not in text and package_v1_0_0 not in text and package_new not in text:
        raise RuntimeError("Could not find Package identity")
    text = text.replace(package_old, package_new, 1)
    text = text.replace(package_v1_0_0, package_new, 1)
    if "Mozc AI v1.0 local-only package" not in text:
        text = text.replace(
            package_new,
            "    <!-- Mozc AI v1.0 local-only package: no cloud inference backend. -->\n"
            + package_new,
            1,
        )
    text = text.replace(
        '<SummaryInformation Keywords="Installer" Description="Mozc インストーラー" Manufacturer="Google LLC" Codepage="932" />',
        f'<SummaryInformation Keywords="Installer" Description="{PRODUCT_NAME} インストーラー" Manufacturer="{MANUFACTURER}" Codepage="932" />',
        1,
    )
    text = text.replace('<Feature Id="MozcInstall" Title="Mozc" Level="1">',
                        f'<Feature Id="MozcInstall" Title="{PRODUCT_NAME}" Level="1">', 1)
    text = text.replace(
        '<StandardDirectory Id="ProgramFilesFolder">\n      <Directory Id="MozcDir" Name="Mozc">',
        # Keep the compile-time Mozc directory name.  The installer helper
        # resolves TIP registration through SystemUtil, whose OSS product
        # directory is Program Files\\Mozc.  The user-facing product identity
        # remains "Mozc AI".
        '<StandardDirectory Id="ProgramFiles64Folder">\n      <Directory Id="MozcDir" Name="Mozc">',
        1,
    )
    text = text.replace(
        '<StandardDirectory Id="ProgramFiles64Folder">\n      <Directory Id="MozcDir" Name="Mozc AI">',
        '<StandardDirectory Id="ProgramFiles64Folder">\n      <Directory Id="MozcDir" Name="Mozc">',
        1,
    )

    upgrade_old = '<Upgrade Id="$(var.UpgradeCode)">'
    if upgrade_old in text:
        text = text.replace(upgrade_old, f'<Upgrade Id="{UPGRADE_CODE}">', 1)
    legacy = f"""    <!-- Migrate an existing upstream/legacy Mozc installation. -->
    <Upgrade Id="{LEGACY_MOZC_UPGRADE_CODE}">
      <UpgradeVersion Minimum="0.0.0.0" IncludeMinimum="yes" Maximum="99.0.0" IncludeMaximum="yes" OnlyDetect="no" Property="UPGRADING" />
    </Upgrade>

"""
    if LEGACY_MOZC_UPGRADE_CODE not in text:
        marker = "    <UI>\n"
        if marker not in text:
            raise RuntimeError("Could not find legacy migration insertion point")
        text = text.replace(marker, legacy + marker, 1)

    if 'ComponentGroupRef Id="AIRuntimeComponents"' not in text:
        marker = '      <ComponentRef Id="CreditsEn" />'
        if marker not in text:
            raise RuntimeError("Could not find Feature runtime insertion point")
        text = text.replace(
            marker,
            marker + '\n      <ComponentGroupRef Id="AIRuntimeComponents" />',
            1,
        )

    if '<ComponentRef Id="LegacyPersonalRerankCleanup" />' not in text:
        marker = '<ComponentRef Id="PrelaunchProcessesV1" />'
        if marker not in text:
            raise RuntimeError("Could not find startup component reference")
        text = text.replace(
            marker,
            marker + '\n      <ComponentRef Id="LegacyPersonalRerankCleanup" />',
            1,
        )

    # Do not reuse the upstream component/value identity: the legacy product
    # removes its identically named Run value during migration.
    text = text.replace(
        '<ComponentRef Id="PrelaunchProcesses" />',
        '<ComponentRef Id="PrelaunchProcessesV1" />',
        1,
    )
    text = text.replace(
        '<Component Id="PrelaunchProcesses" Directory="TARGETDIR">',
        '<Component Id="PrelaunchProcessesV1" Directory="TARGETDIR">',
        1,
    )

    run_marker = (
        '<RegistryValue Id="RunBroker" Root="HKLM" '
        'Key="Software\\Microsoft\\Windows\\CurrentVersion\\Run" '
        'Name="Mozc Prelauncher" Action="write" Type="string" '
        'Value="&quot;[MozcDir]mozc_broker.exe&quot; --mode=prelaunch_processes" />'
    )
    runtime_run = (
        '<RegistryValue Id="RunAIRuntime" Root="HKLM" '
        'Key="Software\\Microsoft\\Windows\\CurrentVersion\\Run" '
        'Name="Mozc AI Runtime" Action="write" Type="string" '
        'Value="&quot;[MozcDir]ai\\rerank_daemon.exe&quot;" />'
    )
    if "RunAIRuntime" not in text:
        if run_marker not in text:
            raise RuntimeError("Could not find prelaunch registry entry")
        text = text.replace(run_marker, runtime_run + "\n      " + run_marker, 1)

    if '<Component Id="LegacyPersonalRerankCleanup"' not in text:
        cleanup_values = (
            "MOZC_RERANK_ENABLED",
            "MOZC_RERANK_DAEMON_ADDR",
            "MOZC_RERANK_TIMEOUT_MS",
            "MOZC_RERANK_GUARD_MODE",
            "MOZC_RERANK_LOG",
            "MOZC_RERANK_HOOK_CMD",
            "MOZC_RERANK_POLICY",
            "MOZC_RERANK_TAU",
            "MOZC_RERANK_CAND_CAP",
            "MOZC_RERANK_MAX_LEN",
        )
        removals = [
            '      <RemoveRegistryValue Id="RemoveLegacyPersonalRun" Root="HKCU" '
            'Key="Software\\Microsoft\\Windows\\CurrentVersion\\Run" '
            'Name="MozcAIRerank" />'
        ]
        removals.extend(
            f'      <RemoveRegistryValue Id="RemoveLegacyEnv{index}" Root="HKCU" '
            f'Key="Environment" Name="{name}" />'
            for index, name in enumerate(cleanup_values, start=1)
        )
        cleanup_component = (
            '    <!-- Remove the pre-v1 WSL/personal startup path and its logging overrides. -->\n'
            '    <Component Id="LegacyPersonalRerankCleanup" Directory="TARGETDIR">\n'
            '      <RegistryValue Id="LegacyCleanupMarker" Root="HKCU" '
            'Key="Software\\MozcAI" Name="LegacyCleanupVersion" '
            f'Value="{PRODUCT_VERSION}" Type="string" KeyPath="yes" />\n'
            + "\n".join(removals)
            + "\n    </Component>\n"
        )
        marker = '    <Component Id="PrelaunchProcessesV1" Directory="TARGETDIR">'
        if marker not in text:
            raise RuntimeError("Could not find startup component insertion point")
        text = text.replace(marker, cleanup_component + marker, 1)
    text = text.replace(
        'RegistryValue Id="RunBroker" Root="HKLM"',
        'RegistryValue Id="RunBrokerV1" Root="HKLM"',
        1,
    )
    text = text.replace(
        'Name="Mozc Prelauncher" Action="write"',
        'Name="Mozc AI Prelauncher" Action="write"',
        1,
    )

    text = text.replace(
        "新しいバージョンの Mozc が既にインストールされています。",
        "新しいバージョンの Mozc AI が既にインストールされています。",
        1,
    )
    print(f"patch {source}")
    if not dry_run:
        source.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Integrate the Mozc AI v1 all-in-one Windows installer"
    )
    parser.add_argument("--mozc-dir", required=True, help="Path to mozc/src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    project = Path(__file__).resolve().parents[1]
    mozc_src = Path(args.mozc_dir).resolve()
    if not (mozc_src / "win32" / "installer" / "installer_oss_64bit.wxs").exists():
        print("error: Mozc Windows installer files not found", file=sys.stderr)
        return 1

    runtime_dir = copy_runtime(project, mozc_src, args.dry_run)
    generate_runtime_fragment(runtime_dir, mozc_src, args.dry_run)
    patch_data_installer_build(mozc_src, args.dry_run)
    patch_installer_build(mozc_src, args.dry_run)
    patch_build_installer_py(mozc_src, args.dry_run)
    patch_installer_wxs(mozc_src, args.dry_run)
    print("All-in-one installer integration complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
