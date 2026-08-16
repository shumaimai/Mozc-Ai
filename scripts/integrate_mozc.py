#!/usr/bin/env python3
"""Integrate the local-only v1 reranker into an upstream Mozc checkout."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

REWRITER_FILES = (
    "rerank_rewriter.h",
    "rerank_rewriter.cc",
    "rerank_rewriter_test.cc",
    "context_clip.h",
    "context_clip.cc",
    "rerank_guard.h",
    "rerank_guard.cc",
    "rerank_eligible_readings.inc",
    "rerank_margin.h",
)


def copy_rewriter_files(project: Path, mozc_src: Path, dry_run: bool) -> None:
    compat = project / "mozc_compat"
    destination = mozc_src / "rewriter"
    for name in REWRITER_FILES:
        src = compat / name
        dst = destination / name
        print(f"copy {src} -> {dst}")
        if not dry_run:
            shutil.copy2(src, dst)


def patch_rewriter_build(project: Path, mozc_src: Path, dry_run: bool) -> None:
    build_file = mozc_src / "rewriter" / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")
    if 'name = "rerank_rewriter"' not in text:
        snippet_lines = (
            project / "mozc_compat" / "rerank_rewriter_build.bazel.patch"
        ).read_text(encoding="utf-8").splitlines()
        start = next(
            i for i, line in enumerate(snippet_lines)
            if line.startswith("mozc_cc_library")
        )
        snippet = "\n".join(snippet_lines[start:]).strip() + "\n\n"
        marker = 'mozc_cc_library(\n    name = "dice_rewriter",'
        if marker not in text:
            raise RuntimeError("Could not find dice_rewriter insertion point")
        text = text.replace(marker, snippet + marker, 1)

    target = '    name = "rewriter",'
    target_start = text.find(target)
    if target_start < 0:
        raise RuntimeError("Could not find aggregate rewriter target")
    deps_start = text.find("    deps = [", target_start)
    deps_end = text.find("    ],", deps_start)
    if deps_start < 0 or deps_end < 0:
        raise RuntimeError("Could not find aggregate rewriter dependencies")
    deps = text[deps_start:deps_end]
    # The v1 product deliberately does not link the legacy cloud AIRewriter.
    deps = deps.replace('        ":ai_rewriter",\n', "")
    if '":rerank_rewriter"' not in deps:
        marker = '        ":a11y_description_rewriter",\n'
        if marker not in deps:
            raise RuntimeError("Could not find dependency insertion point")
        deps = deps.replace(marker, marker + '        ":rerank_rewriter",\n', 1)
    text = text[:deps_start] + deps + text[deps_end:]

    print(f"patch {build_file}")
    if not dry_run:
        build_file.write_text(text, encoding="utf-8")


def strip_legacy_cloud_rewriter(text: str) -> str:
    """Remove old AIRewriter registration while leaving its source unlinked."""
    replacements = (
        ("\n#define MOZC_AI_REWRITER\n", "\n"),
        (
            '#ifdef MOZC_AI_REWRITER\n'
            '#include "rewriter/ai_rewriter.h"\n'
            '#endif  // MOZC_AI_REWRITER\n',
            "",
        ),
        (
            '#ifdef MOZC_AI_REWRITER\n'
            '  AddRewriter(std::make_unique<AIRewriter>());\n'
            '#endif  // MOZC_AI_REWRITER\n',
            "",
        ),
    )
    for old, new in replacements:
        text = text.replace(old, new)
    return text


def patch_rewriter_cc(mozc_src: Path, dry_run: bool) -> None:
    source = mozc_src / "rewriter" / "rewriter.cc"
    text = strip_legacy_cloud_rewriter(source.read_text(encoding="utf-8"))

    if "#define MOZC_RERANK_REWRITER" not in text:
        marker = "#define MOZC_USER_HISTORY_REWRITER\n"
        if marker not in text:
            raise RuntimeError("Could not find rewriter define insertion point")
        text = text.replace(
            marker, marker + "\n#define MOZC_RERANK_REWRITER\n", 1
        )

    if '"rewriter/rerank_rewriter.h"' not in text:
        marker = "#ifdef MOZC_USER_HISTORY_REWRITER\n"
        include = (
            "#ifdef MOZC_RERANK_REWRITER\n"
            '#include "rewriter/rerank_rewriter.h"\n'
            "#endif  // MOZC_RERANK_REWRITER\n\n"
        )
        if marker not in text:
            raise RuntimeError("Could not find rewriter include insertion point")
        text = text.replace(marker, include + marker, 1)

    if "std::make_unique<RerankRewriter>()" not in text:
        marker = (
            "  AddRewriter(make_unique_from_tuples<A11yDescriptionRewriter>(\n"
            "      data_manager.GetA11yDescriptionRewriterData()));\n"
            "}"
        )
        replacement = marker[:-1] + (
            "#ifdef MOZC_RERANK_REWRITER\n"
            "  AddRewriter(std::make_unique<RerankRewriter>());\n"
            "#endif  // MOZC_RERANK_REWRITER\n"
            "}"
        )
        if marker not in text:
            raise RuntimeError("Could not find final rewriter insertion point")
        text = text.replace(marker, replacement, 1)

    print(f"patch {source}")
    if not dry_run:
        source.write_text(text, encoding="utf-8")


def patch_engine_converter_context(mozc_src: Path, dry_run: bool) -> None:
    header = mozc_src / "engine" / "engine_converter.h"
    header_text = header.read_text(encoding="utf-8")
    if "latest_client_context_" not in header_text:
        marker = (
            "  std::shared_ptr<const commands::Request> request_;\n"
            "  std::shared_ptr<const config::Config> config_;\n"
        )
        replacement = marker + (
            "\n  // Surrounding text captured at composition start.\n"
            "  commands::Context latest_client_context_;\n"
        )
        if marker not in header_text:
            raise RuntimeError("Could not find EngineConverter field insertion point")
        header_text = header_text.replace(marker, replacement, 1)
        print(f"patch {header}")
        if not dry_run:
            header.write_text(header_text, encoding="utf-8")

    source = mozc_src / "engine" / "engine_converter.cc"
    text = source.read_text(encoding="utf-8")
    changed = False
    start = "void EngineConverter::OnStartComposition(const commands::Context& context) {\n"
    if start + "  latest_client_context_ = context;\n" not in text:
        if start not in text:
            raise RuntimeError("Could not find OnStartComposition")
        text = text.replace(start, start + "  latest_client_context_ = context;\n", 1)
        changed = True

    suggest = (
        "bool EngineConverter::SuggestWithPreferences(\n"
        "    const composer::Composer& composer, const commands::Context& context,\n"
        "    const ConversionPreferences& preferences) {\n"
    )
    if suggest + "  latest_client_context_ = context;\n" not in text:
        if suggest not in text:
            raise RuntimeError("Could not find SuggestWithPreferences")
        text = text.replace(
            suggest, suggest + "  latest_client_context_ = context;\n", 1
        )
        changed = True

    builder = (
        "  SetRequestType(ConversionRequest::CONVERSION, options);\n"
        "  const ConversionRequest conversion_request =\n"
        "      ConversionRequestBuilder()\n"
        "          .SetComposer(composer)\n"
        "          .SetRequestView(*request_)\n"
        "          .SetConfigView(*config_)\n"
    )
    with_context = builder.replace(
        "          .SetConfigView(*config_)\n",
        "          .SetContextView(latest_client_context_)\n"
        "          .SetConfigView(*config_)\n",
    )
    if with_context not in text:
        if builder not in text:
            raise RuntimeError("Could not find conversion request builder")
        text = text.replace(builder, with_context, 1)
        changed = True

    # Upstream's debug-only failure diagnostic includes the full preceding
    # text and segment dump.  Some Windows release configurations still emit
    # DLOG output, so retain only non-content metadata in this local-first
    # package.
    sensitive_log = (
        '    DLOG(WARNING) << "preceding_text: " << preceding_text\n'
        '                  << ", segments: " << segments_.DebugString();\n'
    )
    privacy_safe_log = (
        '    DLOG(WARNING) << "history reconstruction metadata: preceding_bytes="\n'
        '                  << preceding_text.size()\n'
        '                  << " segment_count=" << segments_.segments_size();\n'
    )
    if sensitive_log in text:
        text = text.replace(sensitive_log, privacy_safe_log, 1)
        changed = True
    if changed:
        print(f"patch {source}")
        if not dry_run:
            source.write_text(text, encoding="utf-8")


def copy_runtime_smoke_client(project: Path, mozc_src: Path, dry_run: bool) -> None:
    client = mozc_src / "client"
    src = project / "mozc_compat" / "runtime_smoke_client.cc"
    dst = client / "runtime_smoke_client.cc"
    print(f"copy {src} -> {dst}")
    if not dry_run:
        shutil.copy2(src, dst)

    build_file = client / "BUILD.bazel"
    text = build_file.read_text(encoding="utf-8")
    if 'name = "runtime_smoke_client"' not in text:
        if '    "mozc_cc_binary",\n' not in text:
            text = text.replace(
                '    "mozc_cc_library",\n',
                '    "mozc_cc_binary",\n    "mozc_cc_library",\n',
                1,
            )
        snippet = (
            project / "mozc_compat" / "runtime_smoke_client_build.bazel.patch"
        ).read_text(encoding="utf-8").strip()
        text = text.rstrip() + "\n\n" + snippet + "\n"
        print(f"patch {build_file}")
        if not dry_run:
            build_file.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Integrate the Mozc AI v1 local-only reranker"
    )
    parser.add_argument("--mozc-dir", required=True, help="Path to mozc/src")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    project = Path(__file__).resolve().parents[1]
    mozc_src = Path(args.mozc_dir).resolve()
    if not (mozc_src / "MODULE.bazel").exists():
        print("error: invalid Mozc source directory", file=sys.stderr)
        return 1

    copy_rewriter_files(project, mozc_src, args.dry_run)
    patch_rewriter_build(project, mozc_src, args.dry_run)
    patch_rewriter_cc(mozc_src, args.dry_run)
    patch_engine_converter_context(mozc_src, args.dry_run)
    copy_runtime_smoke_client(project, mozc_src, args.dry_run)
    print("Local-only reranker integration complete.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
