# Copyright 2024 AI Mozc IME Project
# BUILD file for Rewriter module

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

# Rewriter Interface
cc_library(
    name = "rewriter_interface",
    hdrs = ["rewriter_interface.h"],
)

# AI Rewriter
cc_library(
    name = "ai_rewriter",
    srcs = ["ai_rewriter.cc"],
    hdrs = ["ai_rewriter.h"],
    defines = ["ENABLE_AI_REWRITER"],
    deps = [
        ":rewriter_interface",
        "//ai:ai_backend",
        "//ai:ai_candidate_cache",
        "//ai:ai_config",
        "//ai:ai_logger",
        "//ai:ai_worker",
    ],
)

# Tests

cc_test(
    name = "ai_rewriter_test",
    srcs = ["ai_rewriter_test.cc"],
    deps = [
        ":ai_rewriter",
        "@com_google_googletest//:gtest_main",
    ],
)
