#!/usr/bin/env python3
"""Guard the explicit Ventus Zfinx ISA and register-domain contract."""

from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
VENTUS_ISA = "rv32ima_zfinx_zve32f"

SCALAR_F32_WRITERS = [
    "fadd_s.h",
    "fcvt_s_l.h",
    "fcvt_s_lu.h",
    "fcvt_s_w.h",
    "fcvt_s_wu.h",
    "fdiv_s.h",
    "fmadd_s.h",
    "fmax_s.h",
    "fmin_s.h",
    "fmsub_s.h",
    "fmul_s.h",
    "fnmadd_s.h",
    "fnmsub_s.h",
    "fsgnj_s.h",
    "fsgnjn_s.h",
    "fsgnjx_s.h",
    "fsqrt_s.h",
    "fsub_s.h",
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def check_parser(binary: pathlib.Path, failures: list[str]) -> None:
    positive_profiles = [
        VENTUS_ISA,
        "rv32if_zve32f",
        "rv32ima_zfinx",
        "rv32gcv_zfh",
    ]
    negative_profiles = ["rv32ima_zve32f", "rv32imaf_zfinx_zve32f"]

    for profile in positive_profiles:
        result = subprocess.run(
            [str(binary), f"--isa={profile}"],
            input="DASM(00000013)\n",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode != 0 or result.stdout.strip() != "nop":
            failures.append(f"Spike rejected valid ISA profile {profile}")

    for profile in negative_profiles:
        result = subprocess.run(
            [str(binary), f"--isa={profile}"],
            input="DASM(00000013)\n",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode == 0:
            failures.append(f"Spike accepted invalid ISA profile {profile}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--spike-dasm",
        type=pathlib.Path,
        help="also exercise the built ISA parser through spike-dasm",
    )
    parser.add_argument(
        "--spike-build", type=pathlib.Path,
        help="compile and run VF regext instruction regression against this local build",
    )
    args = parser.parse_args()

    failures: list[str] = []
    parser_header = read("riscv/isa_parser.h")
    parser_source = read("riscv/isa_parser.cc")
    decode = read("riscv/decode.h")
    csrs = read("riscv/csrs.cc")
    vector = read("riscv/v_ext_macros.h")
    device = read("spike_main/spike_device.cc")
    workgroup = read("gvmref/workgroup.cc")

    for token in ["EXT_ZFINX", "EXT_ZVE32X", "EXT_ZVE32F"]:
        if token not in parser_header or token not in parser_source:
            failures.append(f"ISA parser wiring is missing {token}")

    if "READ_FREG_F(reg)" not in decode or "EXT_ZFINX" not in decode:
        failures.append("scalar single-precision reads do not select XPR/FPR")
    if "WRITE_FRD_F(value)" not in decode:
        failures.append("scalar single-precision writes do not select XPR/FPR")
    if "#define require_fp \\\n  require(p->extension_enabled(EXT_ZFINX)" not in decode:
        failures.append("Zfinx execution still depends on mstatus.FS")
    if "!proc->extension_enabled(EXT_ZFINX)" not in csrs:
        failures.append("Zfinx status still exposes or dirties independent FPR state")
    if "||\n        proc->extension_enabled(EXT_ZFINX)))" not in csrs:
        failures.append("Zfinx does not authorize fcsr access")
    if "VFP_SCALAR(width, reg_num)" not in vector:
        failures.append("vector-scalar operands lack an extension-selected reader")
    if "VECTOR_FP32_ENABLED" not in vector or "EXT_ZVE32F" not in vector:
        failures.append("vector float32 execution is not gated by Zve32f")
    if "READ_FREG(rs1_num)" in vector:
        failures.append("a common vector .vf path bypasses the selected reader")

    for filename in SCALAR_F32_WRITERS:
        source = read(f"riscv/insns/{filename}")
        if "require_either_extension('F', EXT_ZFINX)" not in source:
            failures.append(f"{filename} is not enabled for Zfinx")
        if "WRITE_FRD_F(" not in source:
            failures.append(f"{filename} bypasses the selected destination register")

    for source_name, source in [
        ("spike_device.cc", device),
        ("workgroup.cc", workgroup),
    ]:
        if f'isa = "{VENTUS_ISA}"' not in source:
            failures.append(f"{source_name} does not default to {VENTUS_ISA}")
        if 'std::getenv("VENTUS_SPIKE_ISA")' not in source:
            failures.append(f"{source_name} lacks the ISA override")

    if args.spike_dasm:
        if not args.spike_dasm.is_file():
            failures.append(f"spike-dasm not found: {args.spike_dasm}")
        else:
            check_parser(args.spike_dasm.resolve(), failures)

    if args.spike_build:
        build = args.spike_build.resolve()
        compiler = shutil.which("c++")
        if compiler is None or not (build / "libspike_main.so").is_file():
            failures.append("VF regext regression requires c++ and built libspike_main.so")
        else:
            with tempfile.TemporaryDirectory(prefix="ventus-vf-regext-") as temporary:
                binary = pathlib.Path(temporary) / "vf-regext"
                include_dirs = [build, ROOT, *(ROOT / name for name in
                                ("riscv", "fesvr", "softfloat", "fdt", "disasm"))]
                command = [compiler, "-std=c++17", "-O2",
                           *(f"-I{path}" for path in include_dirs),
                           str(ROOT / "tests/ventus_zfinx_vf_regext_test.cc"),
                           f"-L{build}", f"-Wl,-rpath,{build}",
                           "-lspike_main", "-lsoftfloat", "-o", str(binary)]
                env = os.environ.copy()
                env["LD_LIBRARY_PATH"] = str(build) + ":" + env.get("LD_LIBRARY_PATH", "")
                result = subprocess.run(command, env=env, check=False)
                if result.returncode:
                    failures.append("VF regext regression failed to compile")
                elif subprocess.run([str(binary)], env=env, check=False).returncode:
                    failures.append("VF regext instruction regression failed")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print("Ventus Zfinx ISA and register-domain checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
