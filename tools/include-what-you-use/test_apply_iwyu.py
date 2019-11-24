#!/usr/bin/env python3

import apply_iwyu
import enum
import pathlib
import typing
import unittest


class TestParseIncludePaths(unittest.TestCase):
    def test_gcc_nix_linux(self) -> None:
        # $ g++ -Wp,-v -E -o /dev/null -x c++ /dev/null
        gcc_output = """ignoring duplicate directory "/nix/store/xw06hkwpxyjxil3d5h374imxp3qhkscq-python3-3.7.4/include"
ignoring nonexistent directory "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../x86_64-unknown-linux-gnu/include"
ignoring duplicate directory "/nix/store/ff2jmrs1a75cd7bisff2sr2gkndnk50x-glibc-2.27-dev/include"
ignoring duplicate directory "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/include-fixed"
#include "..." search starts here:
#include <...> search starts here:
 /nix/store/xw06hkwpxyjxil3d5h374imxp3qhkscq-python3-3.7.4/include
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0/x86_64-unknown-linux-gnu
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0/backward
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/include
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/include
 /nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/include-fixed
 /nix/store/ff2jmrs1a75cd7bisff2sr2gkndnk50x-glibc-2.27-dev/include
End of search list.

"""
        include_paths = apply_iwyu.parse_include_paths_from_verbose_compiler_output(
            gcc_output
        )
        self.assertEqual(
            include_paths.angle_paths,
            (
                pathlib.Path(
                    "/nix/store/xw06hkwpxyjxil3d5h374imxp3qhkscq-python3-3.7.4/include"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0/x86_64-unknown-linux-gnu"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/../../../../include/c++/9.2.0/backward"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/include"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/include"
                ),
                pathlib.Path(
                    "/nix/store/w3mhqy1nvb117lbbwj18lf09z3akzi04-gcc-9.2.0/lib/gcc/x86_64-unknown-linux-gnu/9.2.0/include-fixed"
                ),
                pathlib.Path(
                    "/nix/store/ff2jmrs1a75cd7bisff2sr2gkndnk50x-glibc-2.27-dev/include"
                ),
            ),
        )

    def test_clang_macos_cxxtrace(self) -> None:
        clang_output = """clang -cc1 version 8.0.0 based upon LLVM 8.0.0 default target x86_64-apple-darwin16.7.0
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include/c++/v1"
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/local/include"
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/Library/Frameworks"
ignoring duplicate directory "/Users/strager/tmp/llvm/clang+llvm-8.0.0-x86_64-apple-darwin/include/c++/v1"
#include "..." search starts here:
#include <...> search starts here:
 ../lib/include
 ../vendor/googletest/googlemock/include
 ../vendor/googletest/googlemock
 ../vendor/googletest/googletest/include
 ../vendor/googletest/googletest
 /Users/strager/tmp/llvm/clang+llvm-8.0.0-x86_64-apple-darwin/include/c++/v1
 /Users/strager/Projects/cxxtrace/local_include
 /Users/strager/tmp/llvm/clang+llvm-8.0.0-x86_64-apple-darwin/lib/clang/8.0.0/include
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/System/Library/Frameworks (framework directory)
End of search list.
"""
        include_paths = apply_iwyu.parse_include_paths_from_verbose_compiler_output(
            clang_output
        )
        self.assertEqual(
            include_paths.angle_paths,
            (
                pathlib.Path("../lib/include"),
                pathlib.Path("../vendor/googletest/googlemock/include"),
                pathlib.Path("../vendor/googletest/googlemock"),
                pathlib.Path("../vendor/googletest/googletest/include"),
                pathlib.Path("../vendor/googletest/googletest"),
                pathlib.Path(
                    "/Users/strager/tmp/llvm/clang+llvm-8.0.0-x86_64-apple-darwin/include/c++/v1"
                ),
                pathlib.Path("/Users/strager/Projects/cxxtrace/local_include"),
                pathlib.Path(
                    "/Users/strager/tmp/llvm/clang+llvm-8.0.0-x86_64-apple-darwin/lib/clang/8.0.0/include"
                ),
                pathlib.Path(
                    "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include"
                ),
            ),
        )

    def test_clang_macos_xcode(self) -> None:
        # $ SDKROOT=/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk \
        #   /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++ \
        #   -Wp,-v -E -o /dev/null -x c++ /dev/null
        clang_output = """clang -cc1 version 8.1.0 (clang-802.0.42) default target x86_64-apple-darwin16.7.0
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include/c++/v1"
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/local/include"
ignoring nonexistent directory "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/Library/Frameworks"
#include "..." search starts here:
#include <...> search starts here:
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../lib/clang/8.1.0/include
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include
 /Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/System/Library/Frameworks (framework directory)
End of search list.
"""
        include_paths = apply_iwyu.parse_include_paths_from_verbose_compiler_output(
            clang_output
        )
        self.assertEqual(
            include_paths.angle_paths,
            (
                pathlib.Path(
                    "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1"
                ),
                pathlib.Path(
                    "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../lib/clang/8.1.0/include"
                ),
                pathlib.Path(
                    "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include"
                ),
                pathlib.Path(
                    "/Users/strager/Applications/Xcode_8.3.3.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/usr/include"
                ),
            ),
        )


class TestFixIWYUCommand(unittest.TestCase):
    def test_simple_command_with_no_include_paths_is_noop(self) -> None:
        fixed_command = apply_iwyu.fix_iwyu_command(
            command=["c++", "-o", "file.o", "file.cpp"],
            include_paths=apply_iwyu.IncludePaths(angle_paths=()),
        )
        self.assertEqual(fixed_command, ["c++", "-o", "file.o", "file.cpp"])

    def test_fixing_strips_pch_includes(self) -> None:
        fixed_command = apply_iwyu.fix_iwyu_command(
            command=[
                "c++",
                "-o",
                "file.o",
                "-include",
                "my_prefix_file.hxx",
                "file.cpp",
            ],
            include_paths=apply_iwyu.IncludePaths(angle_paths=()),
        )
        self.assertEqual(fixed_command, ["c++", "-o", "file.o", "file.cpp"])

    def test_fixing_rewrites_header_search_path_i_to_isystem(self) -> None:
        fixed_command = apply_iwyu.fix_iwyu_command(
            command=["c++", "-o", "file.o", "file.cpp", "-Ifirst_dir", "-Isecond_dir"],
            include_paths=apply_iwyu.IncludePaths(
                angle_paths=(pathlib.Path("first_dir"), pathlib.Path("second_dir"))
            ),
        )
        self.assertEqual(
            fixed_command,
            [
                "c++",
                "-o",
                "file.o",
                "file.cpp",
                "-isystem",
                "first_dir",
                "-isystem",
                "second_dir",
            ],
        )

    def test_fixing_keeps_header_search_path_isystem(self) -> None:
        fixed_command = apply_iwyu.fix_iwyu_command(
            command=[
                "c++",
                "-o",
                "file.o",
                "file.cpp",
                "-isystem",
                "first_dir",
                "-isystem",
                "second_dir",
            ],
            include_paths=apply_iwyu.IncludePaths(
                angle_paths=(pathlib.Path("first_dir"), pathlib.Path("second_dir"))
            ),
        )
        self.assertEqual(
            fixed_command,
            [
                "c++",
                "-o",
                "file.o",
                "file.cpp",
                "-isystem",
                "first_dir",
                "-isystem",
                "second_dir",
            ],
        )

    def test_fixing_drops_missing_header_search_paths(self) -> None:
        fixed_command = apply_iwyu.fix_iwyu_command(
            command=[
                "c++",
                "-o",
                "file.o",
                "file.cpp",
                "-Ifirst_dir",
                "-isystem",
                "second_dir",
            ],
            include_paths=apply_iwyu.IncludePaths(
                angle_paths=(pathlib.Path("third_dir"),)
            ),
        )
        self.assertEqual(
            fixed_command, ["c++", "-o", "file.o", "file.cpp", "-isystem", "third_dir"]
        )


if __name__ == "__main__":
    unittest.main()
