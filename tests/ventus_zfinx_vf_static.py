#!/usr/bin/env python3
"""Guard the Ventus Zfinx register domain used by vector .vf operands."""

from __future__ import annotations

import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> int:
    macros = (ROOT / "riscv/v_ext_macros.h").read_text(encoding="utf-8")
    failures: list[str] = []

    if "#define VENTUS_VFP_SCALAR(width, reg_num)" not in macros:
        failures.append("missing the Ventus Zfinx scalar materialization helper")
    if "READ_REG(reg_num)" not in macros:
        failures.append("Ventus .vf scalar operands are not read from X registers")
    if "VFP_VF_PARAMS(width)" not in macros or \
            "VENTUS_VFP_SCALAR(width, rs1_num)" not in macros:
        failures.append("the common vector .vf path bypasses the Zfinx helper")
    if "READ_FREG(rs1_num)" in macros:
        failures.append("a vector .vf path still reads its scalar from an F register")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print("Ventus Zfinx vector-scalar register-domain checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
