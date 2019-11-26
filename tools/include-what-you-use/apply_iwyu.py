#!/usr/bin/env python3
"""Repair #include directives in cxxtrace's source code.

apply_iwyu requires include-what-you-use <https://include-what-you-use.org/> to
be installed. In particular, the executables fix_includes.py and iwyu_tool.py
should be in PATH.
"""

import argparse
import contextlib
import enum
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
    cxxtrace_project_path / "test" / "concurrency_test_runner.cpp",
    cxxtrace_project_path / "test" / "test_chrome_trace_event_format.cpp",
    cxxtrace_project_path / "test" / "test_processor_id.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "dtrace.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "main.cpp",
    cxxtrace_project_path / "test" / "test_processor_id_manual" / "sample_checker.cpp",
    cxxtrace_project_path / "test" / "test_snapshot.cpp",
    cxxtrace_project_path / "test" / "test_span_thread.cpp",
    cxxtrace_project_path / "test" / "test_thread.cpp",
}

# TODO(strager): IWYU mangles these files in harmful ways which can't be worked
# around using IWYU pragmas. Remove these files from the blacklist when IWYU
# stop breaking code.
blacklisted_cxxtrace_outputs = {
    cxxtrace_project_path
    / "lib"
    / "include"
    / "cxxtrace"
    / "detail"
    / "real_synchronization.h",
    cxxtrace_project_path / "lib" / "include" / "cxxtrace" / "span.h",
    cxxtrace_project_path / "test" / "test_chrome_trace_event_format.cpp",
    cxxtrace_project_path / "test" / "test_string.cpp",
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
        compile_cwd = pathlib.Path(compile_cwd_raw)

        include_paths = get_include_paths_from_real_compiler_invocation(
            command=compile_command, cwd=compile_cwd
        )
        iwyu_command = [iwyu_tool.IWYU_EXECUTABLE] + compile_command[1:]
        iwyu_command = fix_iwyu_command(
            command=iwyu_command, include_paths=include_paths
        )

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
    iwyu_mapping_files = ("libcxx.imp", "gcc.stl.headers.imp", "stl.c.headers.imp")
    project_mapping_files = (
        "apple.imp",
        "glibc.imp",
        "c++.imp",
        "libc++.imp",
        "libstdc++.imp",
        "posix.imp",
        "vendor-benchmark.imp",
        "vendor-cdschecker.imp",
        "vendor-googletest.imp",
        "vendor-nlohmann-json.imp",
        "vendor-relacy.imp",
    )

    share_path = include_what_you_use_share_path()
    iwyu_args = (
        ["--no_default_mappings", "--no_fwd_decls", "--quoted_includes_first"]
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
    command: typing.Sequence[str], include_paths: "IncludePaths"
) -> typing.List[str]:
    """Hackily work around deficiencies in include-what-you-use.

    Munge the given command so include-what-you-use functions correctly.
    """

    fixed_command = command[0:1]

    arg_index = 1
    while arg_index < len(command):
        arg = command[arg_index]

        # HACK(strager): include-what-you-use ignores -isysroot and is unaware
        # of the compiler's implicit header search paths. (For example, because
        # include-what-you-use is Clang-based, implicit search paths
        # can differ if the build command is for GCC.) Drop all -I, -isystem,
        # etc. options, using include_paths instead as the source of truth.
        if arg.startswith("-I"):
            if arg == "-I":
                raise NotImplementedError("'-I /directory' not implemented")
            arg_index += 1
            continue
        if arg in ("-iframework", "-isysroot", "-isystem"):
            arg_index += 2
            continue

        # HACK(strager): include-what-you-use crashes when given a pre-compiled
        # header. Strip any PCH files.
        if arg == "-include":
            arg_index += 1
            included_file = command[arg_index]
            arg_index += 1
            is_pch_include = included_file.endswith(".hxx")
            if not is_pch_include:
                fixed_command.append("-include")
                fixed_command.append(included_file)
            continue

        fixed_command.append(arg)
        arg_index += 1

    for path in include_paths.angle_paths:
        # NOTE(strager): If a header is found through -I, include-what-you-use
        # suggests #include "header.h" instead of #include <header.h>. We want
        # the latter form, so always emit -isystem and never -I.
        fixed_command.append("-isystem")
        fixed_command.append(str(path))
    return fixed_command


def get_include_paths_from_real_compiler_invocation(
    command: typing.Sequence[str], cwd: pathlib.Path
) -> "IncludePaths":
    command = list(command)
    command.extend(("-Wp,-v", "-E", "-o", "/dev/null"))
    # HACK(strager): GCC complains if -o is specified twice. When this happens,
    # despite failing with a non-zero exit code, GCC still prints the include
    # paths we want. Only bother checking the exit code if we don't get the
    # output we expect.
    compile_result = subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        encoding="utf-8",
    )
    include_paths = parse_include_paths_from_verbose_compiler_output(
        compile_result.stdout
    )
    if not include_paths.angle_paths:
        compile_result.check_returncode()
        raise Exception(
            f"Failed to parse include paths from output of compiler: {command!r}"
        )
    return include_paths


def parse_include_paths_from_verbose_compiler_output(output: str) -> "IncludePaths":
    class State(enum.Enum):
        ANGLE_PATHS = "ANGLE_PATHS"
        DONE = "DONE"
        INITIAL = "INITIAL"

    angle_paths = []
    state = State.INITIAL
    for line in output.splitlines():
        if state == State.INITIAL:
            if line == "#include <...> search starts here:":
                state = State.ANGLE_PATHS
        elif state == State.ANGLE_PATHS:
            if line.startswith(" "):
                if line.endswith(" (framework directory)"):
                    # Skip macOS framework directories for now.
                    # TODO(strager): Ultimately translate into -iframework.
                    pass
                else:
                    angle_paths.append(pathlib.Path(line[1:]))
            else:
                state = state.DONE
        elif state == State.DONE:
            pass
    return IncludePaths(angle_paths=tuple(angle_paths))


class IncludePaths(typing.NamedTuple):
    angle_paths: typing.Tuple[pathlib.Path]


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
