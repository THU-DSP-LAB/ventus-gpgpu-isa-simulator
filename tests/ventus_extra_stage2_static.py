#!/usr/bin/env python3
"""Static regression checks for Ventus Stage 2 MMA instruction wiring."""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]

MMA = [
    ("mma_m8n8k16", "mma.m8n8k16", "0x0000000a", "0x0e00007f"),
    ("mma_m16n8k16", "mma.m16n8k16", "0x0200000a", "0x0e00007f"),
    ("mma_m8n16k16", "mma.m8n16k16", "0x0400000a", "0x0e00007f"),
    ("mma_m16n16k16", "mma.m16n16k16", "0x0600000a", "0x0e00007f"),
    ("mma_m8n8k8", "mma.m8n8k8", "0x0800000a", "0x0e00007f"),
    ("mma_m16n8k8", "mma.m16n8k8", "0x0a00000a", "0x0e00007f"),
    ("mma_m8n16k8", "mma.m8n16k8", "0x0c00000a", "0x0e00007f"),
    ("mma_m16n16k8", "mma.m16n16k8", "0x0e00000a", "0x0e00007f"),
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
    custom = (
        read("riscv/ventus_custom.h")
        + read("riscv/ventus_custom.cc")
        + ((ROOT / "riscv" / "ventus_mma.h").read_text(encoding="utf-8")
           if (ROOT / "riscv" / "ventus_mma.h").exists() else "")
    )

    require("0x0a ? 4" in decode or "0x0a ||" in decode,
            "insn_length does not force opcode 0x0a to 32 bits", failures)
    require("ventus_mma.h" in riscv_mk, "ventus_mma.h missing from riscv headers", failures)
    require("void ventus_exec_mma(processor_t *p, insn_t insn)" in custom,
            "ventus_exec_mma declaration/definition missing", failures)

    for insn, disasm_name, match, mask in MMA:
        macro = insn.upper()
        require(f"#define MATCH_{macro} {match}" in encoding,
                f"MATCH_{macro} missing or changed", failures)
        require(f"#define MASK_{macro} {mask}" in encoding,
                f"MASK_{macro} missing or changed", failures)
        require(f"DECLARE_INSN({insn}, MATCH_{macro}, MASK_{macro})" in encoding,
                f"DECLARE_INSN({insn}) missing", failures)
        require(re.search(rf"^\s*{re.escape(insn)}\s*\\", riscv_mk, re.MULTILINE) is not None,
                f"{insn} missing from riscv.mk.in instruction list", failures)
        header = ROOT / "riscv" / "insns" / f"{insn}.h"
        require(header.exists(), f"riscv/insns/{insn}.h missing", failures)
        if header.exists():
            require("ventus_exec_mma(p, insn);" in header.read_text(encoding="utf-8"),
                    f"riscv/insns/{insn}.h does not call ventus_exec_mma", failures)
        require(f'"{disasm_name}"' in disasm, f"disassembler missing {disasm_name}", failures)
        require(f'add_ventus_mma_insn(this, "{disasm_name}"' in disasm,
                f"{disasm_name} does not use MMA-specific formatter", failures)

    require("add_ventus_mma_insn" in disasm,
            "disassembler missing MMA-specific formatter", failures)
    require('add_vector_vv_insn(this, "mma.' not in disasm,
            "MMA disassembler must not use RVV vv formatter with vm suffix", failures)

    forbidden = [
        "mma-sim",
        "dependencies/unfu",
        "cmodel_mma",
        "sfu_core",
        "LUT_PATH",
        "mock",
        "VENTUS_UNFU",
    ]
    for needle in forbidden:
        require(needle not in custom, f"forbidden dependency or mock marker found: {needle}", failures)

    for symbol in [
        "VentusMMAShape",
        "VentusMMAInputType",
        "VentusMMAOutputType",
        "VentusMMARegisterFile",
        "ExecutionState",
        "check_full_warp_state",
        "execute_mma_register_file",
        "get_state()->sstatus->enabled(SSTATUS_VS)",
        "p->VU.vill",
        "p->VU.vstart->read()",
        "p->VU.VLEN",
    ]:
        require(symbol in custom, f"local MMA helper symbol missing: {symbol}", failures)

    require("for (reg_t lane = 0; lane < VENTUS_CUSTOM_LANES; ++lane)" in custom,
            "MMA executor does not visibly write a fixed 32-lane result", failures)

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    print("Ventus Stage 2 static checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
