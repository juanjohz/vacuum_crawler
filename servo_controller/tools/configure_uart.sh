#!/usr/bin/env bash
# Run locally with sudo. Does not reboot automatically.
set -euo pipefail
if [[ $EUID -ne 0 ]]; then
  echo "Run with sudo: sudo bash $0" >&2
  exit 1
fi
python3 - <<'PY'
from pathlib import Path
from datetime import datetime
import shutil
p = Path('/boot/firmware/cmdline.txt')
original = p.read_text()
tokens = original.split()
if 'console=tty1' not in tokens:
    raise SystemExit('Expected console=tty1; refusing to change unexpected boot configuration')
if 'console=serial0,115200' in tokens:
    backup = p.with_name('cmdline.txt.before-teensy-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    shutil.copy2(p, backup)
    p.write_text(' '.join(t for t in tokens if t != 'console=serial0,115200') + '\n')
    print('Backup:', backup)
else:
    print('UART console boot argument already absent')
assert 'console=serial0,115200' not in p.read_text().split()
print('Boot command line:', p.read_text().strip())
PY
systemctl disable --now serial-getty@ttyAMA0.service
# Mask prevents systemd's getty generator from reactivating it.
systemctl mask serial-getty@ttyAMA0.service
if systemctl is-active --quiet serial-getty@ttyAMA0.service; then
  echo 'Serial login service is still active' >&2
  exit 1
fi
echo 'UART login disabled and masked. Reboot with sudo reboot when ready.'
echo 'After reboot: cat /proc/consoles'
echo 'Then: systemctl is-active serial-getty@ttyAMA0.service (expect inactive)'
echo 'To restore login later, systemctl unmask first, then enable the service.'
