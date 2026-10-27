"""Check both RoboClaw USB connections without moving motors."""

import sys

from basicmicro import Basicmicro


# Port numbers may change after reconnecting USB devices.
CONTROLLERS = [
    ("/dev/ttyACM0", 0x81),
    ("/dev/ttyACM1", 0x80),
]

BAUD_RATE = 38400


def check_connection(port, address):
    controller = Basicmicro(
        port,
        BAUD_RATE,
        timeout=0.2,
        retries=2,
    )

    try:
        print(f"Checking {port}, address {address:#04x}...")

        if not controller.Open():
            print("  FAILED: could not open serial port.")
            return False

        success, version = controller.ReadVersion(address)

        if not success:
            print("  FAILED: no valid firmware response.")
            return False

        print(f"  CONNECTED: {version.strip()}")
        return True

    except Exception as error:
        print(f"  ERROR: {error}")
        return False

    finally:
        controller.close()


def main():
    results = [
        check_connection(port, address)
        for port, address in CONTROLLERS
    ]

    print(f"\nConnected: {sum(results)}/{len(results)} controllers")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())