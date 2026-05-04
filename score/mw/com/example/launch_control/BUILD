load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "launch_control_message",
    srcs = ["launch_control_message.cpp"],
    hdrs = [
        "launch_control_message.h",
        "datatype.h",
    ],
)

cc_library(
    name = "launch_control_management",
    srcs = ["launch_control_management.cpp"],
    hdrs = ["launch_control_management.h"],
    deps = [":launch_control_message"],
)

cc_library(
    name = "launch_control_interface",
    hdrs = ["launch_control_interface.h"],
    deps = [":launch_control_message"],
)

cc_library(
    name = "launch_control_gen_cpp",
    srcs = ["launch_control_gen.cpp", "launch_control_rust_wrapper.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":launch_control_interface",
        "//score/mw/com/impl/rust:bridge_macros",  # <--- aggiunta: espone bridge_macros.h
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:registry_bridge_macro_cpp",
    ],
    alwayslink = True,
)

cc_library(
    name = "ping_pong_message",
    hdrs = ["ping_pong_message.h"],
)

cc_library(
    name = "ping_pong_interface",
    hdrs = ["ping_pong_interface.h"],
    deps = [":ping_pong_message"],
)

cc_library(
    name = "ping_pong_gen_cpp",
    srcs = ["ping_pong_gen.cpp", "ping_pong_rust_wrapper.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":ping_pong_interface",
        "//score/mw/com/impl/rust:bridge_macros",
        "//score/mw/com/impl/rust/com-api/com-api-ffi-lola:registry_bridge_macro_cpp",
    ],
    alwayslink = True,
)


cc_binary(
    name = "launch_control_sender_cpp",
    srcs = ["main_sender.cpp"],
    data = ["etc/mw_com_config.json"],
    deps = [
        ":launch_control_gen_cpp",
        ":launch_control_interface",
        ":launch_control_management",
        ":launch_control_message",
        ":ping_pong_gen_cpp",
        ":ping_pong_interface",
        ":ping_pong_message",
        "//score/mw/com",                      # espone score/mw/com/types.h
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "launch_control_receiver_cpp",
    srcs = ["main_receiver.cpp"],
    data = ["etc/mw_com_config.json"],
    deps = [
        ":launch_control_gen_cpp",
        ":launch_control_interface",
        ":launch_control_message",
        ":ping_pong_gen_cpp",
        ":ping_pong_interface",
        ":ping_pong_message",
        "//score/mw/com",                      # espone score/mw/com/types.h
        "@score_baselibs//score/mw/log",
    ],
)
