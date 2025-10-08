# tests/data/comprehensive_min.s
# Round-trip coverage for: ADD, SUB, ADDI, LW, SW, BEQ, LUI, AUIPC, JAL, JALR
# No assembler directives (.section/.globl/.balign) so your parser won't complain.

_start:
    # R-type
    ADD   x5, x6, x7          # expect 0x007302b3
    SUB   x5, x6, x7          # expect 0x407302b3

    # I-type (arith + load)
    ADDI  x5, x6, -1          # expect 0xfff30293
    LW    x5, 8(x6)           # expect 0x00832  283

    # S-type
    SW    x7, 12(x6)          # expect 0x00732623

    # B-type forward to b_fwd: target should be +8 bytes (2 instrs) from BEQ's PC
    BEQ   x5, x6, b_fwd       # expect 0x00628663
    ADDI  x0, x0, 0           # NOP filler (4 B)
    ADDI  x0, x0, 0           # NOP filler (8 B) -> label lands here

b_fwd:
    ADDI  x0, x0, 0

    # U-type
    LUI   x5, 0xABCDE         # expect 0xabcde2b7  (your LUI rule: low 20 bits into U field)
    AUIPC x5, 0x12345000      # expect 0x12345297  (your AUIPC rule: (v>>12) in U field)

    # J-type forward to j_fwd: +16 bytes (4 instrs) from JAL's PC
    JAL   x1, j_fwd           # expect 0x014000ef
    ADDI  x0, x0, 0           # +4
    ADDI  x0, x0, 0           # +8
    ADDI  x0, x0, 0           # +12
    ADDI  x0, x0, 0           # +16 -> label lands here

j_fwd:
    ADDI  x0, x0, 0
    # I-type JALR via register (not PC-relative)
    JALR  x1, x5, 4           # expect 0x004280e7

# ---- Immediate boundary tests ------------------------------------------------

bounds:
    # I-type bounds: [-2048, +2047]
    ADDI  x1, x2, -2048
    ADDI  x3, x4, 2047

    # LW bounds
    LW    x5, -2048(x6)
    LW    x7, 2047(x8)

    # SW bounds
    SW    x9, -2048(x10)
    SW    x11, 2047(x12)

# Small backward/forward branches (±8 bytes, aligned)
bk_2:
    ADDI  x0, x0, 0
    ADDI  x0, x0, 0
    BEQ   x5, x5, bk_2        # back -8 bytes

fw_2:
    ADDI  x0, x0, 0
    BEQ   x5, x5, fw_3        # forward +4 bytes
fw_3:
    ADDI  x0, x0, 0

# PC-relative branch to earlier gap (varied spacing)
gap0:
    ADDI  x0, x0, 0
    ADDI  x0, x0, 0
    ADDI  x0, x0, 0
    ADDI  x0, x0, 0

gap1:
    BEQ   x1, x1, gap0        # small backward branch

# Call/return sanity (link in x2)
call_here:
    JAL   x2, after_call
    ADDI  x0, x0, 0
    ADDI  x0, x0, 0
after_call:
    ADDI  x0, x0, 0           # fall-through after link
