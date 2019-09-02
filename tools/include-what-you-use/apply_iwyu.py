#!/usr/bin/env python3
"""Repair #include directives in cxxtrace's source code.

apply_iwyu requires include-what-you-use <https://include-what-you-use.org/> to
be installed. In particular, the executables fix_includes.py and iwyu_tool.py
should be in PATH.
"""

import argparse
import contextlib
import functools
import importlib.util
import io
import os
import os.path
import pathlib
import shlex
import shutil
import subprocess
import sys
import types
import typing

# Source: https://github.com/include-what-you-use/include-what-you-use/blob/3b10ae16a5de06812122e64e18419298b588e1f3/iwyu_globals.h#L34
IWYU_EXIT_SUCCESS_OFFSET = 2

cxxtrace_project_path = pathlib.Path(__file__).resolve(strict=True).parent.parent.parent
include_what_you_use_tools_path = pathlib.Path(__file__).resolve(strict=True).parent
top_level_cxxtrace_source_directories = (
    cxxtrace_project_path / "example",
    cxxtrace_project_path / "lib",
    cxxtrace_project_path / "test",
)

# TODO(strager): IWYU crashes on these source files. Remove these files from the
# blacklist when they stop crashing IWYU.
blacklisted_cxxtrace_inputs = {
    cxxtrace_project_path / "example" / "group_and_sort_parallel.cpp",
    cxxtrace_project_path / "test" / "event.cpp",
    cxxtrace_project_path / "test" / "exhaustive_rng.cpp",
    cxxtrace_project_path / "test" / "nlohmann_json.cpp",
    cxxtrace_project_path / "test" / "test_add.cpp",
    cxxtrace_project_path / "test" / "test_chrome_trace_event_format.cpp",
    cxxtrace_project_path / "test" / "test_clock.cpp",
    cxxtrace_project_path / "test" / "test_exhaustive_rng.cpp",
    cxxtrace_project_path / "test" / "test_molecular.cpp",
    cxxtrace_project_path / "test" / "test_processor_id.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "dtrace.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "gnuplot.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "main.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "sample_checker.cpp",
    cxxtrace_project_path / "test" / "test_ring_queue.cpp",
    cxxtrace_project_path / "test" / "test_snapshot.cpp",
    cxxtrace_project_path / "test" / "test_span.cpp",
    cxxtrace_project_path / "test" / "test_span_thread.cpp",
    cxxtrace_project_path / "test" / "test_thread.cpp",
    cxxtrace_project_path / "test" / "thread.cpp",
}

# TODO(strager): IWYU mangles these files in harmful ways which can't be worked
# around using IWYU pragmas. Remove these files from the blacklist when IWYU
# stop breaking code.
blacklisted_cxxtrace_outputs = {
    cxxtrace_project_path / "lib" / "include" / "cxxtrace" / "span.h"
}

iwyu_fix_includes = None
iwyu_tool = None


def main() -> None:
    args = parse_arguments()

    global iwyu_fix_includes
    global iwyu_tool
    try:
        iwyu_fix_includes = import_module_from_path(
            file_name="fix_includes.py", module_name="iwyu_fix_includes"
        )
        iwyu_tool = import_module_from_path(
            file_name="iwyu_tool.py", module_name="iwyu_tool"
        )
    except ModuleNotFoundError as e:
        sys.stderr.write(f"fatal: {e}\n")
        exit(1)

    if args.source_files:
        cwd = pathlib.Path(os.getcwd())
        source_files = [cwd / f for f in args.source_files]
    else:
        source_files = sorted(
            frozenset(
                flattened(
                    source_dir.glob("**/*.cpp")
                    for source_dir in top_level_cxxtrace_source_directories
                )
            )
            - blacklisted_cxxtrace_inputs
        )

    compilation_db: CompilationDatabase = iwyu_tool.parse_compilation_db(
        compilation_db_path=args.cmake_build_directory
    )
    compilation_db = iwyu_tool.slice_compilation_db(
        compilation_db=compilation_db, selection=source_files
    )
    compute_and_apply_fixes_for_compilation_database(compilation_db)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(usage=__doc__)
    parser.add_argument(
        "--cmake-build-directory",
        dest="cmake_build_directory",
        help="path to a cxxtrace build directory containing compile_commands.json",
        metavar="DIR",
        type=pathlib.Path,
    )
    parser.add_argument(
        "source_files",
        nargs="*",
        help="path to a .cpp file to analyze",
        metavar="FILE",
        type=pathlib.Path,
    )
    return parser.parse_args()


CompilationDatabase = typing.Iterable["CompilationDatabaseEntry"]
CompilationDatabaseEntry = typing.Any


def compute_and_apply_fixes_for_compilation_database(
    compilation_db: CompilationDatabase
) -> None:
    file_whitelist = (
        frozenset(
            flattened(
                source_dir.glob("**/*")
                for source_dir in top_level_cxxtrace_source_directories
            )
        )
        - blacklisted_cxxtrace_outputs
    )

    cwd = os.getcwd()

    @functools.lru_cache(maxsize=None)
    def get_clang_resource_dir(clang_exe: str) -> pathlib.Path:
        clang_output = subprocess.check_output(
            [clang_exe, "--print-resource-dir"], encoding="utf-8"
        )
        return pathlib.Path(clang_output.rstrip("\n\r"))

    for entry in compilation_db:
        compile_command_raw = entry["command"]
        if not isinstance(compile_command_raw, str):
            raise TypeError("'command' in compilation database should be a string")
        compile_cwd_raw = entry["directory"]
        if not isinstance(compile_cwd_raw, str):
            raise TypeError("'directory' in compilation database should be a string")

        compile_command = iwyu_tool.split_command(compile_command_raw)
        if not compile_command:
            raise TypeError("'command' in compilation database should be non-empty")

        source_file = pathlib.Path(compile_command[-1])  # HACK(strager)
        relative_source_file = relative_path(source_file, cwd)
        sys.stderr.write(f"Checking {relative_source_file} ...\n")

        iwyu_command = [iwyu_tool.IWYU_EXECUTABLE] + compile_command[1:]
        clang_resource_dir = get_clang_resource_dir(compile_command[0])
        iwyu_command = fix_iwyu_command(
            command=iwyu_command, clang_resource_dir=clang_resource_dir
        )

        compile_cwd = pathlib.Path(compile_cwd_raw)
        fixes = compute_fixes(
            iwyu_command=iwyu_command,
            command_cwd=compile_cwd,
            file_whitelist=file_whitelist,
        )
        sys.stderr.buffer.write(fixes.iwyu_output)
        sys.stderr.write(f"Applying fixes related to {relative_source_file} ...\n")
        apply_fixes(fixes=fixes, file_whitelist=file_whitelist)


def compute_fixes(
    iwyu_command: typing.Sequence[str],
    command_cwd: pathlib.Path,
    file_whitelist: typing.Collection[pathlib.Path],
) -> "IWYUFixes":
    iwyu_mapping_files = ("libcxx.imp", "stl.c.headers.imp")
    project_mapping_files = (
        "apple.imp",
        "c++.imp",
        "libc++.imp",
        "posix.imp",
        "vendor-benchmark.imp",
        "vendor-cdschecker.imp",
        "vendor-relacy.imp",
    )

    share_path = include_what_you_use_share_path()
    iwyu_args = (
        ["--no_default_mappings", "--quoted_includes_first"]
        + [
            f"--mapping_file={include_what_you_use_tools_path / f}"
            for f in project_mapping_files
        ]
        + [f"--mapping_file={share_path / f}" for f in iwyu_mapping_files]
        + [
            f"--check_also={relative_path(f, start=command_cwd)}"
            for f in file_whitelist
        ]
    )
    extra_args = list(flattened(("-Xiwyu", arg) for arg in iwyu_args))

    return run_iwyu(iwyu_command=iwyu_command + extra_args, command_cwd=command_cwd)


def apply_fixes(
    fixes: "IWYUFixes", file_whitelist: typing.Collection[pathlib.Path]
) -> None:
    fix_includes_flags = FixIncludesFlags(
        basedir=fixes.cwd,
        blank_lines=False,
        comments=False,
        dry_run=False,
        ignore_re=None,
        keep_iwyu_namespace_format=False,
        only_re=None,
        reorder=False,
        safe_headers=False,
        separate_project_includes=False,
    )
    with io.StringIO(fixes.iwyu_output.decode("utf-8")) as iwyu_output_file:
        with contextlib.redirect_stdout(sys.stderr):
            iwyu_fix_includes.ProcessIWYUOutput(
                f=iwyu_output_file,
                files_to_process=map(str, file_whitelist),
                flags=fix_includes_flags,
                cwd=fixes.cwd,
            )


class FixIncludesFlags(typing.NamedTuple):
    basedir: typing.Optional[pathlib.Path]
    blank_lines: bool
    comments: bool
    dry_run: bool
    ignore_re: None
    keep_iwyu_namespace_format: bool
    only_re: None
    reorder: bool
    safe_headers: bool
    separate_project_includes: bool


def run_iwyu(
    iwyu_command: typing.Sequence[str], command_cwd: pathlib.Path
) -> "IWYUFixes":
    def command_failed(result: subprocess.CompletedProcess) -> typing.NoReturn:
        human_shell_command = "cd {cwd} && {command}".format(
            cwd=shlex.quote(str(command_cwd)),
            command=" ".join(map(shlex.quote, iwyu_command)),
        )
        output = result.stdout
        sys.stderr.write(f"\nfatal: command failed:\n$ {human_shell_command}\n")
        sys.stderr.buffer.write(output)
        sys.stderr.buffer.flush()
        exit(1)

    result = subprocess.run(
        iwyu_command,
        cwd=command_cwd,
        shell=False,
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE,
    )
    if result.returncode >= IWYU_EXIT_SUCCESS_OFFSET:
        return IWYUFixes(iwyu_output=result.stdout, cwd=command_cwd)
    else:
        command_failed(result)


class IWYUFixes(typing.NamedTuple):
    cwd: pathlib.Path
    iwyu_output: bytes


def fix_iwyu_command(
    command: typing.Sequence[str], clang_resource_dir: pathlib.Path
) -> typing.List[str]:
    """Hackily work around deficiencies in include-what-you-use.

    Munge the given command so include-what-you-use functions correctly.
    """

    fixed_command = command[0:1]

    fixed_command.append("-isystem")
    fixed_command.append(str(clang_resource_dir / "include"))

    arg_index = 1
    while arg_index < len(command):
        arg = command[arg_index]

        # HACK(strager): If a header is found through -I, include-what-you-use
        # suggests #include "header.h" instead of #include <header.h>. We want
        # the latter form, so replace -I/directory with -isystem /directory.
        if arg.startswith("-I"):
            if arg == "-I":
                raise NotImplementedError("'-I /directory' not implemented")
            fixed_command.append("-isystem")
            fixed_command.append(arg[len("-I") :])
            arg_index += 1
            continue

        # HACK(strager): include-what-you-use ignores -isysroot. Replace
        # -isysroot with -isystem.
        if arg == "-isysroot":
            fixed_command.append("-isystem")
            arg_index += 1
            fixed_command.append(
                str(pathlib.Path(command[arg_index]) / "usr" / "include")
            )
            arg_index += 1
            continue

        fixed_command.append(arg)
        arg_index += 1

    return fixed_command


def include_what_you_use_share_path() -> pathlib.Path:
    # HACK(strager): Assume iwyu_tool.py is in ${prefix}/bin, and that .imp
    # files are in ${prefix}/share/include-what-you-use.
    prefix = pathlib.Path(iwyu_tool.__file__).resolve(strict=True).parent.parent
    return prefix / "share" / "include-what-you-use"


def import_module_from_path(module_name: str, file_name: str) -> types.ModuleType:
    path = shutil.which(file_name)
    if path is None:
        raise ModuleNotFoundError(f"Could not find {file_name} in PATH")
    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    spec_loader = spec.loader
    assert spec_loader is not None
    spec_loader.exec_module(module)
    return module


def relative_path(path: pathlib.Path, start: pathlib.Path) -> pathlib.Path:
    return pathlib.Path(os.path.relpath(path, start=start))


_T = typing.TypeVar("_T")


def flattened(iterables: typing.Iterable[typing.Iterable[_T]]) -> typing.Iterable[_T]:
    for iterable in iterables:
        for item in iterable:
            yield item


if __name__ == "__main__":
    main()
