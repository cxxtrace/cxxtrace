#!/usr/bin/env python3

import apply_iwyu
import enum
import pathlib
import typing
import unittest


class TestParseIncludePaths(unittest.TestCase):
    def test_gcc(self) -> None:
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
        self.assertEqual(include_paths.quote_paths, ())
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

    # @nocommit also clang plz!


if __name__ == "__main__":
    unittest.main()
