#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Host round-trip test for np05j-boot-repack using real NP05J v4 header layout."""

import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def run(cmd: list[str]) -> str:
    res = subprocess.run(cmd, check=True, text=True, capture_output=True)
    return res.stdout


def main() -> int:
    repo = Path(__file__).resolve().parents[4]
    repack = repo / "Build" / "nubia_np05jPkg" / "AndroidHelper" / "np05j-boot-repack-host"
    selector = repo / "Build" / "nubia_np05jPkg" / "BootSelect" / "np05j-bootselect.bin"
    shim = repo / "Build" / "nubia_np05jPkg" / "BootSelect" / "np05j-reloc-shim.bin"
    uefi = repo / "Build" / "nubia_np05jPkg" / "nubia_np05j-kernel"
    real_hdr_src = repo / "Device_np05j" / "np05j-boot_b-first64k.bin"

    for p in [repack, selector, shim, uefi, real_hdr_src]:
        assert p.is_file(), f"Missing artifact: {p}"

    # Construct synthetic stock Boot v4 image inheriting NP05J's real v4 header layout
    hdr_page = bytearray(real_hdr_src.read_bytes()[:4096])
    synth_kernel_size = 131072
    synth_ramdisk_size = 16384
    struct.pack_into("<II", hdr_page, 8, synth_kernel_size, synth_ramdisk_size)

    synth_kernel = bytes(((i * 37 + 13) & 0xFF) for i in range(synth_kernel_size))
    synth_ramdisk = bytes(((i * 53 + 7) & 0xFF) for i in range(synth_ramdisk_size))
    avb_tail = b"AVB_TEST_TAIL_MARKER" + bytes(4096 - 20)

    stock_bytes = bytes(hdr_page) + synth_kernel + synth_ramdisk + avb_tail
    stock_sha = hashlib.sha256(stock_bytes).hexdigest()

    with tempfile.TemporaryDirectory(prefix="np05j_repack_test_") as td:
        tmp = Path(td)
        stock_img = tmp / "stock-boot.img"
        comb_img = tmp / "combined-boot.img"
        rest_img = tmp / "restored-boot.img"
        stock_img.write_bytes(stock_bytes)

        out = run([str(repack), "inspect", str(stock_img)])
        assert "header_version=4" in out
        assert "sunuefi_combined=0" in out

        run([
            str(repack), "repack",
            "--stock", str(stock_img),
            "--selector", str(selector),
            "--shim", str(shim),
            "--uefi", str(uefi),
            "--out", str(comb_img),
            "--timeout", "3",
        ])

        out = run([str(repack), "inspect", str(comb_img)])
        assert "sunuefi_combined=1" in out
        assert "next_boot=none" in out

        for tgt in ["uefi", "bios", "windows", "linux", "android"]:
            run([str(repack), "request", str(comb_img), tgt, "1"])
            out = run([str(repack), "inspect", str(comb_img)])
            assert f"next_boot={tgt} " in out, out

        run([str(repack), "clear-request", str(comb_img)])
        out = run([str(repack), "inspect", str(comb_img)])
        assert "next_boot=none" in out

        run([str(repack), "restore", str(comb_img), str(rest_img)])
        rest_bytes = rest_img.read_bytes()
        rest_sha = hashlib.sha256(rest_bytes).hexdigest()
        assert len(rest_bytes) == len(stock_bytes)
        assert rest_sha == stock_sha
        assert rest_bytes == stock_bytes

    print(f"PASS: test_boot_repack (BOOT v4 repack -> NEXTv1 target cycle -> lossless restore sha256={stock_sha[:16]}...)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
