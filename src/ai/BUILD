# Copyright 2024 AI Mozc IME Project
# BUILD file for AI module

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

# Common compiler options
COPTS = [
    "-std=c++17",
    "-Wall",
    "-Wextra",
]

# Proto library for AI configuration
proto_library(
    name = "ai_config_proto_lib",
    srcs = ["ai_config.proto"],
)

cc_proto_library(
    name = "ai_config_proto",
    deps = [":ai_config_proto_lib"],
)

# AI Configuration Manager
cc_library(
    name = "ai_config",
    srcs = ["ai_config.cc"],
    hdrs = ["ai_config.h"],
    copts = COPTS,
    includes = ["."],
    deps = [
        # ":ai_config_proto",  # Enable when using protobuf
    ],
)

# AI Logger
cc_library(
    name = "ai_logger",
    srcs = ["ai_logger.cc"],
    hdrs = ["ai_logger.h"],
    copts = COPTS,
    includes = ["."],
    deps = [
        ":ai_config",
    ],
)

# AI Candidate Cache
cc_library(
    name = "ai_candidate_cache",
    srcs = ["ai_candidate_cache.cc"],
    hdrs = ["ai_candidate_cache.h"],
    copts = COPTS,
    includes = ["."],
)

# AI Backend Interface and Implementations
cc_library(
    name = "ai_backend",
    srcs = [
        "ollama_backend.cc",
        "mock_backend.cc",
    ],
    hdrs = ["ai_backend.h"],
    copts = COPTS,
    includes = ["."],
    deps = [
        ":ai_config",
    ],
    linkopts = select({
        "@platforms//os:windows": [
            "-lws2_32",
            "-lwinhttp",
        ],
        "//conditions:default": [],
    }),
)

# AI Worker
cc_library(
    name = "ai_worker",
    srcs = ["ai_worker.cc"],
    hdrs = ["ai_worker.h"],
    copts = COPTS,
    includes = ["."],
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
    ],
)

# Combined AI library
cc_library(
    name = "ai",
    copts = COPTS,
    includes = ["."],
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
        ":ai_worker",
    ],
)

# Tests

cc_test(
    name = "ai_candidate_cache_test",
    srcs = ["ai_candidate_cache_test.cc"],
    copts = COPTS,
    deps = [
        ":ai_candidate_cache",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ai_config_test",
    srcs = ["ai_config_test.cc"],
    copts = COPTS,
    deps = [
        ":ai_config",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ai_backend_test",
    srcs = ["ai_backend_test.cc"],
    copts = COPTS,
    deps = [
        ":ai_backend",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ai_worker_test",
    srcs = ["ai_worker_test.cc"],
    copts = COPTS,
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_worker",
        "@com_google_googletest//:gtest_main",
    ],
)
