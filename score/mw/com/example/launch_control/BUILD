load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "launch_control_message",
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
    srcs = ["launch_control_gen.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":launch_control_interface",
    ],
    implementation_deps = [
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
    ],
)
