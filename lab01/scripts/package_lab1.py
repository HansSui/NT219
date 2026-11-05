#!/usr/bin/env python3
"""Build the Lab 1 submission archive.

Stages the source, report, evidence and (optionally) one Windows and one Linux
binary, fills the identity fields in the staged report, writes SUBMISSION.md,
and zips everything under output/package/ (gitignored).

Examples:
    python scripts/package_lab1.py --student "Nguyen Van A" \
        --student-id 25520001 --class-name NT219.Q11 \
        --windows-bin build/windows-msvc/Release/aestool.exe

    python scripts/package_lab1.py --skip-evidence --skip-binaries
"""
import argparse
import datetime
import os
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "output" / "package" / "lab01-submission.zip"

INCLUDE = [
    "CMakeLists.txt",
    "README.md",
    "REPORT.md",
    "BENCHMARKS.md",
    ".gitignore",
    "src",
    "tests",
    "kat",
    "gui",
    "scripts",
]
IGNORE = shutil.ignore_patterns(
    "__pycache__", "*.pyc", "*.o", "*.obj", ".aestool_nonces", ".aestool_nonces.lock"
)


def copy_tree(src: Path, dst: Path) -> None:
    if src.is_dir():
        shutil.copytree(src, dst, ignore=IGNORE)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)


def fill_identity(report: Path, student: str, student_id: str, class_name: str) -> None:
    text = report.read_text(encoding="utf-8")
    fields = {
        "Sinh viên": student,
        "MSSV": student_id,
        "Lớp": class_name,
    }
    for label, value in fields.items():
        if not value:
            continue
        text = re.sub(
            rf"(\*\*{re.escape(label)}:\*\*).*",
            lambda match: f"{match.group(1)} {value}  ",
            text,
            count=1,
        )
    report.write_text(text, encoding="utf-8")


def submission_markdown(student: str, student_id: str, class_name: str,
                        binaries: list[tuple[str, Path]], has_evidence: bool) -> str:
    today = datetime.date.today().isoformat()
    lines = [
        "# Lab 1 submission",
        "",
        f"- Student: {student or '(fill in)'}",
        f"- Student ID: {student_id or '(fill in)'}",
        f"- Class: {class_name or '(fill in)'}",
        f"- Date: {today}",
        "",
        "## Contents",
        "",
        "- `src/`, `tests/`, `kat/`, `gui/`, `scripts/`, `CMakeLists.txt` - source, tests and KAT vectors",
        "- `REPORT.md` - lab report with measured results and self-grade",
        "- `README.md`, `BENCHMARKS.md` - build, usage and benchmark instructions",
    ]
    if has_evidence:
        lines.append("- `output/` - test logs, negative-case evidence, benchmark CSVs and plots")
    for label, path in binaries:
        lines.append(f"- `bin/{path.name}` - {label} binary")
    lines += [
        "",
        "## Build",
        "",
        "```bash",
        "cmake -S . -B build/ubuntu-gcc",
        "cmake --build build/ubuntu-gcc",
        "(cd build/ubuntu-gcc && ctest --output-on-failure)",
        "./build/ubuntu-gcc/aestool --kat kat/vectors.json",
        "```",
        "",
        "Windows MSVC, MinGW GCC and MinGW Clang use `scripts/build_windows.bat`.",
        "",
        "## Academic integrity",
        "",
        "I confirm that this submission is my own work, that the AES core was",
        "written for this lab on top of Crypto++, and that all reported",
        "measurements come from the committed CSV files. External test vectors",
        "are cited in `REPORT.md` and `kat/vectors.json`.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--student", default="", help="student name for the report")
    parser.add_argument("--student-id", default="", help="student ID / MSSV")
    parser.add_argument("--class-name", default="", help="class / lop")
    parser.add_argument("--windows-bin", default="", help="path to aestool.exe")
    parser.add_argument("--linux-bin", default="", help="path to the Linux aestool")
    parser.add_argument("--skip-binaries", action="store_true", help="do not copy binaries")
    parser.add_argument("--skip-evidence", action="store_true", help="do not copy output/")
    parser.add_argument("--out", default=str(DEFAULT_OUT), help="output zip path")
    args = parser.parse_args()

    out = Path(args.out)
    stage = out.with_suffix("")
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    for name in INCLUDE:
        src = ROOT / name
        if src.exists():
            copy_tree(src, stage / name)

    has_evidence = False
    if not args.skip_evidence:
        evidence = ROOT / "output"
        if evidence.is_dir():
            copy_tree(evidence, stage / "output")
            shutil.rmtree(stage / "output" / "package", ignore_errors=True)
            has_evidence = True

    binaries: list[tuple[str, Path]] = []
    if not args.skip_binaries:
        bin_dir = stage / "bin"
        for label, value in (("Windows x64", args.windows_bin), ("Linux x86_64", args.linux_bin)):
            if not value:
                continue
            src = Path(value)
            if not src.is_absolute():
                src = ROOT / src
            if not src.exists():
                print(f"warning: binary not found, skipping: {src}", file=sys.stderr)
                continue
            bin_dir.mkdir(exist_ok=True)
            shutil.copy2(src, bin_dir / src.name)
            binaries.append((label, src))

    report = stage / "REPORT.md"
    if report.exists():
        fill_identity(report, args.student, args.student_id, args.class_name)
    (stage / "SUBMISSION.md").write_text(
        submission_markdown(args.student, args.student_id, args.class_name, binaries, has_evidence),
        encoding="utf-8",
    )

    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(stage))

    size_mb = out.stat().st_size / (1024 * 1024)
    print(f"wrote {out} ({size_mb:.1f} MiB)")
    print(f"staged tree: {stage}")
    if not (args.student and args.student_id and args.class_name):
        print("identity fields are incomplete; rerun with --student, --student-id and --class-name",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
