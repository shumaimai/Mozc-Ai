# Copyright 2024 AI Mozc IME Project
# BUILD file for Rewriter module

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

# Platform-specific compiler options
# Windows uses MSVC syntax, Linux/macOS uses GCC/Clang syntax
COPTS = select({
    "@platforms//os:windows": [
        "/std:c++17",
        "/W3",
        "/utf-8",  # Required for source files containing Japanese characters
    ],
    "//conditions:default": [
        "-std=c++17",
        "-Wall",
        "-Wextra",
    ],
})

# Rewriter Interface
cc_library(
    name = "rewriter_interface",
    hdrs = ["rewriter_interface.h"],
    copts = COPTS,
    includes = ["."],
)

# AI Rewriter
cc_library(
    name = "ai_rewriter",
    srcs = ["ai_rewriter.cc"],
    hdrs = ["ai_rewriter.h"],
    copts = COPTS,
    defines = ["ENABLE_AI_REWRITER"],
    includes = ["."],
    deps = [
        ":rewriter_interface",
        "//src/ai:ai_backend",
        "//src/ai:ai_candidate_cache",
        "//src/ai:ai_config",
        "//src/ai:ai_logger",
        "//src/ai:ai_worker",
    ],
)

# Tests

cc_test(
    name = "ai_rewriter_test",
    srcs = ["ai_rewriter_test.cc"],
    copts = COPTS,
    deps = [
        ":ai_rewriter",
        "@googletest//:gtest_main",
    ],
)
