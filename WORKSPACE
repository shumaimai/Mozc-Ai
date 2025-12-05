# Copyright 2024 AI Mozc IME Project
# Bazel WORKSPACE file

workspace(name = "ai_mozc")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Google Test
http_archive(
    name = "com_google_googletest",
    sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
    strip_prefix = "googletest-1.14.0",
    urls = ["https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz"],
)

# Abseil (for future use)
http_archive(
    name = "com_google_absl",
    sha256 = "733726b8c3a6d39a4120d7e45ea8b41a434cdacde401cba500f14236c49b39dc",
    strip_prefix = "abseil-cpp-20240116.2",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20240116.2.tar.gz"],
)

# Protocol Buffers (for future use)
http_archive(
    name = "com_google_protobuf",
    sha256 = "d0f5f605d0d656007ce6c8b5a82df3037e1d8fe8b121ed42e536f569dec16113",
    strip_prefix = "protobuf-3.21.12",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/refs/tags/v3.21.12.tar.gz"],
)

# Rules for C++
http_archive(
    name = "rules_cc",
    sha256 = "2037875b9a4456dce4a79d112a8ae885bbc4aad968e6587dca6e64f3a0900cdf",
    strip_prefix = "rules_cc-0.0.9",
    urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz"],
)

# Platform definitions
http_archive(
    name = "platforms",
    sha256 = "8150406605389ececb6da07cbcb509d5637a3ab9a24bc69b1101531367d89d74",
    urls = ["https://github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz"],
)
