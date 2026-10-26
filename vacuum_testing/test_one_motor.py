from basicmicro import Basicmicro
import time

PORT = "/dev/ttyACM0"
ADDRESS = 0x80
BAUD = 38400

DUTY = -15000
DURATION = 2

# Previously verified:
#/dev/ttyACM0 → 0x80
# /dev/ttyACM1 → 0x81
# USB port numbers can change after reconnecting.

rc = Basicmicro(PORT, BAUD)

try:
    if not rc.Open():
        raise RuntimeError(f"Could not open {PORT}")

    print("Running motors...")

    rc.DutyAccelM1(ADDRESS, 0, DUTY)
    rc.DutyAccelM2(ADDRESS, 0, DUTY)

    time.sleep(DURATION)

finally:
    try:
        print("Stopping...")
        rc.DutyAccelM1(ADDRESS, 0, 0)
        rc.DutyAccelM2(ADDRESS, 0, 0)
    finally:
        rc.close()

print("Done.")