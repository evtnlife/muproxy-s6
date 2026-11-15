#!/usr/bin/env python3
"""
Parser básico para pacotes PS/GameGuard observados no fluxo do MU.

Pacote típico visto no dispatcher (opcode 0xF1):
    C1 05 F1 01 01 FD ...

- byte 0: magic 0xC1 (ou outro)
- byte 1: tamanho do payload
- byte 2: opcode principal
- byte 3+: sub-opcode / dados
"""

from dataclasses import dataclass
from typing import Optional


@dataclass
class PsPacket:
    raw: bytes
    magic: int
    opcode: int
    sub_opcode: Optional[int]
    payload: bytes

    def __str__(self) -> str:
        sub = f"0x{self.sub_opcode:02X}" if self.sub_opcode is not None else "-"
        return f"PS magic=0x{self.magic:02X} op=0x{self.opcode:02X} sub={sub} len={len(self.raw)}"


def parse_ps(data: bytes) -> Optional[PsPacket]:
    if len(data) < 3:
        return None
    magic = data[0]
    size = data[1]
    opcode = data[2]
    sub_opcode = data[3] if len(data) > 3 else None
    payload = data[3:]
    return PsPacket(
        raw=data,
        magic=magic,
        opcode=opcode,
        sub_opcode=sub_opcode,
        payload=payload,
    )
