#!/usr/bin/env python3
"""Static regression checks for Ventus Stage 1 custom instruction wiring."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]

PACKED = [
    "vadd_f16x2",
    "vmul_f16x2",
    "vfma_f16x2",
    "vadd_bf16x2",
    "vmul_bf16x2",
    "vfma_bf16x2",
]

CONVERT = [
    "vcvt_fp32_fp16",
    "vcvt_fp16_fp32",
    "vcvt_fp32_bf16",
    "vcvt_bf16_fp32",
]

SFU_F32 = [
    "vex2_approx_f32",
    "vlg2_approx_f32",
    "vrcp_approx_f32",
    "vsqrt_approx_f32",
    "vrsqrt_approx_f32",
    "vsin_approx_f32",
    "vcos_approx_f32",
    "vtanh_approx_f32",
    "vgelu_approx_f32",
    "vsilu_approx_f32",
]

SFU_F16X2 = [
    "vex2_approx_f16x2",
    "vrcp_approx_f16x2",
    "vsqrt_approx_f16x2",
    "vrsqrt_approx_f16x2",
    "vtanh_approx_f16x2",
    "vgelu_approx_f16x2",
    "vsilu_approx_f16x2",
]

SFU_BF16X2 = [
    "vex2_approx_bf16x2",
    "vrcp_approx_bf16x2",
    "vsqrt_approx_bf16x2",
    "vrsqrt_approx_bf16x2",
    "vtanh_approx_bf16x2",
    "vgelu_approx_bf16x2",
    "vsilu_approx_bf16x2",
]

ALL_STAGE1 = PACKED + CONVERT + SFU_F32 + SFU_F16X2 + SFU_BF16X2

DISASM_NAMES = [
    "vadd.f16x2",
    "vmul.f16x2",
    "vfma.f16x2",
    "vadd.bf16x2",
    "vmul.bf16x2",
    "vfma.bf16x2",
    "vcvt.fp32.fp16",
    "vcvt.fp16.fp32",
    "vcvt.fp32.bf16",
    "vcvt.bf16.fp32",
    "vex2.approx.f32",
    "vlg2.approx.f32",
    "vrcp.approx.f32",
    "vsqrt.approx.f32",
    "vrsqrt.approx.f32",
    "vsin.approx.f32",
    "vcos.approx.f32",
    "vtanh.approx.f32",
    "vgelu.approx.f32",
    "vsilu.approx.f32",
    "vex2.approx.f16x2",
    "vrcp.approx.f16x2",
    "vsqrt.approx.f16x2",
    "vrsqrt.approx.f16x2",
    "vtanh.approx.f16x2",
    "vgelu.approx.f16x2",
    "vsilu.approx.f16x2",
    "vex2.approx.bf16x2",
    "vrcp.approx.bf16x2",
    "vsqrt.approx.bf16x2",
    "vrsqrt.approx.bf16x2",
    "vtanh.approx.bf16x2",
    "vgelu.approx.bf16x2",
    "vsilu.approx.bf16x2",
]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def main() -> int:
    failures: list[str] = []
    encoding = read("riscv/encoding.h")
    decode = read("riscv/decode.h")
    riscv_mk = read("riscv/riscv.mk.in")
    disasm = read("disasm/disasm.cc")
    template = read("riscv/insn_template.cc")

    for opcode in ["0x0a", "0x2a", "0x42", "0x5a", "0x5f", "0x72", "0x7a"]:
        require(opcode in decode, f"insn_length does not force {opcode} to 32 bits", failures)

    require('"ventus_custom.h"' in template, "insn template does not include ventus_custom.h", failures)
    require("ventus_custom.h" in riscv_mk, "ventus_custom.h missing from riscv headers", failures)
    require("ventus_custom_arith.h" in riscv_mk, "ventus_custom_arith.h missing from riscv headers", failures)
    require("ventus_custom.cc" in riscv_mk, "ventus_custom.cc missing from riscv sources", failures)

    for insn in ALL_STAGE1:
        macro = insn.upper()
        require(f"MATCH_{macro}" in encoding, f"MATCH_{macro} missing", failures)
        require(f"MASK_{macro}" in encoding, f"MASK_{macro} missing", failures)
        require(f"DECLARE_INSN({insn}," in encoding, f"DECLARE_INSN({insn}) missing", failures)
        require(re.search(rf"^\s*{re.escape(insn)}\s*\\", riscv_mk, re.MULTILINE) is not None,
                f"{insn} missing from riscv.mk.in instruction list", failures)
        require((ROOT / "riscv" / "insns" / f"{insn}.h").exists(),
                f"riscv/insns/{insn}.h missing", failures)

    for name in DISASM_NAMES:
        require(f'"{name}"' in disasm, f"disassembler missing {name}", failures)

    if (ROOT / "riscv" / "ventus_custom.h").exists():
        custom = read("riscv/ventus_custom.h") + read("riscv/ventus_custom_arith.h") + read("riscv/ventus_custom.cc")
        forbidden = ["mma-sim", "dependencies/unfu", "sfu_core", "cmodel_mma", "LUT_PATH", "mock"]
        for needle in forbidden:
            require(needle not in custom, f"forbidden dependency or mock marker found: {needle}", failures)
        required_symbols = [
            "ventus_exec_packed",
            "ventus_exec_vcvt",
            "ventus_exec_sfu",
            "VentusPackedOp",
            "VentusSFUOp",
            "get_state()->sstatus->enabled(SSTATUS_VS)",
            "p->VU.vill",
        ]
        for symbol in required_symbols:
            require(symbol in custom, f"local arithmetic symbol missing: {symbol}", failures)
    else:
        failures.append("riscv/ventus_custom.{h,cc} missing")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    print("Ventus Stage 1 static checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
