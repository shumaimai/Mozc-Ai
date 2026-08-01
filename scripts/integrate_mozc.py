#!/usr/bin/env python3
# Copyright 2024 AI Mozc IME Project
# Apply AI Mozc patches to a Mozc source tree.

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def copy_ai_module(ai_mozc_dir: Path, mozc_src: Path, dry_run: bool) -> None:
    ai_src = ai_mozc_dir / "src" / "ai"
    ai_dst = mozc_src / "ai"
    files = sorted(ai_src.glob("*.h")) + sorted(ai_src.glob("*.cc"))
    build_src = ai_mozc_dir / "mozc_compat" / "ai" / "BUILD.bazel"

    if not dry_run:
        ai_dst.mkdir(parents=True, exist_ok=True)

    for src in files:
        dst = ai_dst / src.name
        print(f"copy {src} -> {dst}")
        if not dry_run:
            shutil.copy2(src, dst)

    print(f"copy {build_src} -> {ai_dst / 'BUILD.bazel'}")
    if not dry_run:
        shutil.copy2(build_src, ai_dst / "BUILD.bazel")


def copy_rewriter_files(ai_mozc_dir: Path, mozc_src: Path, dry_run: bool) -> None:
    compat = ai_mozc_dir / "mozc_compat"
    rewriter_dst = mozc_src / "rewriter"
    for name in ("ai_rewriter.h", "ai_rewriter.cc", "ai_rewriter_test.cc"):
        src = compat / name
        dst = rewriter_dst / name
        print(f"copy {src} -> {dst}")
        if not dry_run:
            shutil.copy2(src, dst)


def patch_rewriter_build(ai_mozc_dir: Path, mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "rewriter" / "BUILD.bazel"
    text = build_file.read_text()

    snippet_file = ai_mozc_dir / "mozc_compat" / "rewriter_build.bazel.patch"
    snippet_lines = snippet_file.read_text().splitlines()
    start = next(i for i, line in enumerate(snippet_lines) if line.startswith("mozc_cc_library"))
    end = next(
        i for i, line in enumerate(snippet_lines)
        if i > start and line.startswith("# Also add")
    )
    snippet = "\n".join(snippet_lines[start:end]).strip() + "\n\n"

    if 'name = "ai_rewriter"' in text:
        print("rewriter/BUILD.bazel already contains ai_rewriter; skipping")
        return

    marker = 'mozc_cc_library(\n    name = "dice_rewriter",'
    if marker not in text:
        raise RuntimeError("Could not find dice_rewriter target in rewriter/BUILD.bazel")

    text = text.replace(marker, snippet + marker, 1)

    rewriter_deps_marker = (
        '    name = "rewriter",\n'
        '    srcs = [\n'
        '        "rewriter.cc",\n'
        '    ],\n'
        '    hdrs = ["rewriter.h"],\n'
        '    visibility = [\n'
        '        "//converter:__pkg__",\n'
        '        "//engine:__pkg__",\n'
        '    ],\n'
        '    deps = [\n'
        '        ":a11y_description_rewriter",\n'
        '        ":calculator_rewriter",'
    )
    rewriter_deps_replacement = (
        '    name = "rewriter",\n'
        '    srcs = [\n'
        '        "rewriter.cc",\n'
        '    ],\n'
        '    hdrs = ["rewriter.h"],\n'
        '    visibility = [\n'
        '        "//converter:__pkg__",\n'
        '        "//engine:__pkg__",\n'
        '    ],\n'
        '    deps = [\n'
        '        ":a11y_description_rewriter",\n'
        '        ":ai_rewriter",\n'
        '        ":calculator_rewriter",'
    )
    if rewriter_deps_marker not in text:
        if '        ":ai_rewriter",\n        ":calculator_rewriter",' in text:
            print("rewriter target already depends on ai_rewriter; skipping deps patch")
        else:
            raise RuntimeError("Could not find rewriter deps insertion point")
    else:
        text = text.replace(rewriter_deps_marker, rewriter_deps_replacement, 1)

    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text)


def patch_rewriter_cc(mozc_src: Path, dry_run: bool) -> None:
    rewriter_cc = mozc_src / "rewriter" / "rewriter.cc"
    text = rewriter_cc.read_text()

    if "MOZC_AI_REWRITER" in text:
        print("rewriter/rewriter.cc already patched; skipping")
        return

    define_block = (
        "#define MOZC_USER_HISTORY_REWRITER\n\n"
        "#define MOZC_AI_REWRITER\n"
    )
    text = text.replace(
        "#define MOZC_USER_HISTORY_REWRITER\n\n\n#ifdef MOZC_COMMAND_REWRITER",
        define_block + "\n#ifdef MOZC_COMMAND_REWRITER",
        1,
    )

    include_block = (
        "#ifdef MOZC_USER_DICTIONARY_REWRITER\n"
        "#include \"rewriter/user_dictionary_rewriter.h\"\n"
        "#endif  // MOZC_USER_DICTIONARY_REWRITER\n\n"
        "#ifdef MOZC_AI_REWRITER\n"
        "#include \"rewriter/ai_rewriter.h\"\n"
        "#endif  // MOZC_AI_REWRITER\n"
    )
    text = text.replace(
        "#ifdef MOZC_USER_DICTIONARY_REWRITER\n"
        "#include \"rewriter/user_dictionary_rewriter.h\"\n"
        "#endif  // MOZC_USER_DICTIONARY_REWRITER\n\n"
        "#ifdef MOZC_USER_HISTORY_REWRITER",
        include_block + "\n#ifdef MOZC_USER_HISTORY_REWRITER",
        1,
    )

    add_block = (
        "  AddRewriter(make_unique_from_tuples<CorrectionRewriter>(\n"
        "      modules, data_manager.GetReadingCorrectionData()));\n"
        "#ifdef MOZC_AI_REWRITER\n"
        "  AddRewriter(std::make_unique<AIRewriter>());\n"
        "#endif  // MOZC_AI_REWRITER\n"
        "  AddRewriter(std::make_unique<T13nPromotionRewriter>());"
    )
    text = text.replace(
        "  AddRewriter(make_unique_from_tuples<CorrectionRewriter>(\n"
        "      modules, data_manager.GetReadingCorrectionData()));\n"
        "  AddRewriter(std::make_unique<T13nPromotionRewriter>());",
        add_block,
        1,
    )

    print(f"patch {rewriter_cc}")
    if not dry_run:
        rewriter_cc.write_text(text)


def main() -> int:
    parser = argparse.ArgumentParser(description="Integrate AI Mozc into google/mozc")
    parser.add_argument("--mozc-dir", required=True, help="Path to mozc/src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    ai_mozc_dir = Path(__file__).resolve().parents[1]
    mozc_src = Path(args.mozc_dir).resolve()

    if not (mozc_src / "MODULE.bazel").exists():
        print(f"error: {mozc_src} does not look like mozc/src", file=sys.stderr)
        return 1

    print(f"AI Mozc: {ai_mozc_dir}")
    print(f"Mozc src: {mozc_src}")
    print(f"Dry run: {args.dry_run}")
    print()

    copy_ai_module(ai_mozc_dir, mozc_src, args.dry_run)
    copy_rewriter_files(ai_mozc_dir, mozc_src, args.dry_run)
    patch_rewriter_build(ai_mozc_dir, mozc_src, args.dry_run)
    patch_rewriter_cc(mozc_src, args.dry_run)

    print()
    print("Integration complete.")
    print("Next:")
    print(f"  cd {mozc_src}")
    print("  bazelisk build //ai:all //rewriter:ai_rewriter //server:mozc_server")
    print("  bazelisk test //ai:all //rewriter:ai_rewriter_test")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
