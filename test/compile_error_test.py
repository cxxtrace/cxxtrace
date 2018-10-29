#!/usr/bin/env python3

import argparse
import pathlib
import shlex
import subprocess
import sys
import tempfile
import typing


def main():
    parser = argparse.ArgumentParser(fromfile_prefix_chars="@")
    parser.add_argument("--cmake", dest="cmake_executable", type=pathlib.Path)
    parser.add_argument(
        "--cmake-variable",
        dest="cmake_variables",
        action="append",
        default=[],
        type=str,
    )
    parser.add_argument("--filecheck", dest="filecheck_executable", type=pathlib.Path)
    parser.add_argument("--source-file", dest="source_file", type=pathlib.Path)
    parser.add_argument(
        "--target-include-directories",
        dest="target_include_directories",
        action="append",
        default=[],
        nargs="*",
        type=pathlib.Path,
    )
    parser.add_argument(
        "--target-link-library",
        dest="target_link_libraries",
        action="append",
        default=[],
        type=pathlib.Path,
    )
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as temp_dir_str:
        temp_dir = pathlib.Path(temp_dir_str)
        project = Project(source_dir=temp_dir / "source", build_dir=temp_dir / "build")
        project.create_project_files(
            source_file=args.source_file,
            target_include_directories=sum(args.target_include_directories, []),
            target_link_libraries=args.target_link_libraries,
        )
        project.cmake_configure(
            cmake_executable=args.cmake_executable, variables=args.cmake_variables
        )
        build_result = project.build(cmake_executable=args.cmake_executable)
        ok = project.check_build_result(
            build_result=build_result,
            source_file=args.source_file,
            filecheck_executable=args.filecheck_executable,
        )
        if not ok:
            exit(1)


class Project:

    def __init__(self, source_dir: pathlib.Path, build_dir: pathlib.Path) -> None:
        super().__init__()
        self.__build_dir = build_dir
        self.__source_dir = source_dir

    def create_project_files(
        self,
        source_file: pathlib.Path,
        target_include_directories: typing.Sequence[pathlib.Path],
        target_link_libraries: typing.Sequence[pathlib.Path],
    ) -> None:
        self.__source_dir.mkdir(exist_ok=True, parents=True)
        cmake_lists = f"""cmake_minimum_required(VERSION 3.10)
project(compile_error_test)
add_executable(compile_error_test {cmake_escape_string(str(source_file))})
target_link_libraries(
    compile_error_test
    PRIVATE
    {cmake_escape_strings(map(str, target_link_libraries))}
)
target_include_directories(
    compile_error_test
    PRIVATE
    {cmake_escape_strings(map(str, target_include_directories))}
)
"""
        cmake_lists_path = self.__source_dir / "CMakeLists.txt"
        cmake_lists_path.write_text(cmake_lists)

        log_running_command(["cat", cmake_lists_path])
        sys.stderr.write(cmake_lists)
        sys.stderr.flush()

    def cmake_configure(
        self, cmake_executable: pathlib.Path, variables: typing.Sequence[str]
    ) -> None:
        self.__build_dir.mkdir(exist_ok=True, parents=True)
        command = [cmake_executable]
        command.extend(f"-D{var}" for var in variables)
        command.append(self.__source_dir)
        cwd = self.__build_dir
        log_running_command(command, cwd=cwd)
        subprocess.check_call(command, cwd=cwd)

    def build(self, cmake_executable: pathlib.Path) -> "BuildResult":
        command = [cmake_executable, "--build", self.__build_dir]
        log_running_command(command)
        result = subprocess.run(
            command, encoding="utf-8", stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        output = result.stdout
        sys.stderr.write(output)
        sys.stderr.flush()
        return BuildResult(output=output, returncode=result.returncode)

    def check_build_result(
        self,
        build_result: "BuildResult",
        source_file: pathlib.Path,
        filecheck_executable: pathlib.Path,
    ) -> bool:
        if build_result.returncode == 0:
            print("error: build succeeded, but it should have failed", file=sys.stderr)
            sys.stderr.flush()
            return False

        command = [filecheck_executable, source_file]
        log_running_command(command)
        with subprocess.Popen(
            command, encoding="utf-8", stdin=subprocess.PIPE
        ) as filecheck:
            filecheck.communicate(build_result.output)
            returncode = filecheck.wait()
        return returncode == 0


class BuildResult(typing.NamedTuple):
    output: str
    returncode: int


def log_running_command(
    command: typing.Sequence[typing.Union[str, pathlib.Path]],
    cwd: typing.Optional[pathlib.Path] = None,
) -> None:
    command_strings = []
    if cwd is not None:
        command_strings.append(f"cd {shlex.quote(str(cwd))}")
    command_strings.append(" ".join(shlex.quote(str(arg)) for arg in command))
    full_command_string = " && ".join(command_strings)
    sys.stderr.write(f"$ {full_command_string}\n")
    sys.stderr.flush()


def cmake_escape_string(string: str) -> str:
    # TODO(strager)
    return string


def cmake_escape_strings(strings: typing.Sequence[str]) -> str:
    return " ".join(map(cmake_escape_string, strings))


if __name__ == "__main__":
    main()
