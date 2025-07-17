import sys
import subprocess
from pathlib import Path

if len(sys.argv) < 2:
    print("Usage: disasm_ami.py <ami_8088_bios.bin> [lines]")
    sys.exit(1)

bios_path = Path(sys.argv[1])

try:
    lines = int(sys.argv[2]) if len(sys.argv) > 2 else None
except ValueError:
    print("Second argument must be an integer (number of lines).")
    sys.exit(1)

if not bios_path.exists():
    print(f"File {bios_path} not found")
    sys.exit(1)

cmd = ["ndisasm", "-b", "16", str(bios_path)]
proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
if proc.returncode != 0:
    sys.stderr.write(proc.stderr)
    sys.exit(proc.returncode)

for i, line in enumerate(proc.stdout.splitlines()):
    if lines is not None and i >= lines:
        break
    print(line)
