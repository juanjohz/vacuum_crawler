from basicmicro import Basicmicro
import time


# Controllers
FRONT_PORT = "/dev/ttyACM1"
BACK_PORT = "/dev/ttyACM0"

FRONT_ADDRESS = 0x81
BACK_ADDRESS = 0x80

BAUD = 38400

# Motor settings: negative duty = forward
DUTY = -15000
DURATION = 2
READ_INTERVAL = 0.1

# Both controllers: M1 = right, M2 = left
front = Basicmicro(FRONT_PORT, BAUD)
back = Basicmicro(BACK_PORT, BAUD)

opened = []

try:
    if not front.Open():
        raise RuntimeError("Could not open front controller")
    opened.append(("Front", front, FRONT_ADDRESS))

    if not back.Open():
        raise RuntimeError("Could not open back controller")
    opened.append(("Back", back, BACK_ADDRESS))

    print("Running all four motors forward...")

    front.DutyAccelM1(FRONT_ADDRESS, 0, DUTY)
    front.DutyAccelM2(FRONT_ADDRESS, 0, DUTY)

    back.DutyAccelM1(BACK_ADDRESS, 0, DUTY)
    back.DutyAccelM2(BACK_ADDRESS, 0, DUTY)

    end_time = time.monotonic() + DURATION

    while time.monotonic() < end_time:
        front_right = front.ReadEncM1(FRONT_ADDRESS)
        front_left = front.ReadEncM2(FRONT_ADDRESS)

        back_right = back.ReadEncM1(BACK_ADDRESS)
        back_left = back.ReadEncM2(BACK_ADDRESS)

        print("----------------")
        print("Front right (M1):", front_right)
        print("Front left  (M2):", front_left)
        print("Back right  (M1):", back_right)
        print("Back left   (M2):", back_left)

        time.sleep(READ_INTERVAL)

except KeyboardInterrupt:
    print("\nTest interrupted.")

finally:
    print("Stopping all motors...")

    for name, controller, address in opened:
        for stop_motor in (
            controller.DutyAccelM1,
            controller.DutyAccelM2,
        ):
            try:
                if not stop_motor(address, 0, 0):
                    print(f"{name}: stop not acknowledged")
            except Exception as error:
                print(f"{name}: stop failed: {error}")

    for controller in (front, back):
        try:
            controller.close()
        except Exception as error:
            print(f"Close failed: {error}")

print("Test completed.")