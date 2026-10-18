"""Check the executable header and boot files inside the raw MODE2 disc."""
from pathlib import Path
import struct
out = Path("output")
exe = (out / "ufc_ps1.exe").read_bytes()
assert exe[:8] == b"PS-X EXE", "Invalid PS1 executable header"
pc, gp, address, size = struct.unpack_from("<4I", exe, 0x10)
assert 0x80000000 <= pc < 0x80200000
assert 0x80000000 <= address < address + size <= 0x80200000
assert len(exe) >= 2048 + size
cue = (out / "Unlimited_Fighting_Champs.cue").read_text()
assert 'Unlimited_Fighting_Champs.bin' in cue and 'MODE2/2352' in cue
raw = (out / "Unlimited_Fighting_Champs.bin").read_bytes()
assert len(raw) % 2352 == 0
iso = b"".join(raw[i+24:i+24+2048] for i in range(0, len(raw), 2352))
assert iso[16*2048+1:16*2048+6] == b"CD001"
assert b"PLAYSTATION" in iso[16*2048:17*2048]
assert exe in iso, "Executable missing or corrupted on disc"
assert Path("SYSTEM.CNF").read_bytes() in iso, "Boot config missing"
print(f"Verified PS-X EXE and BIN/CUE: PC={pc:#x}, payload={size}, sectors={len(raw)//2352}")
