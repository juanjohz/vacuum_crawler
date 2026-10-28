# Vacuum cameras

Publishes two independent CSI camera feeds using `camera_ros`. Requires both
sensors to be detected and a libcamera build with Raspberry Pi 5 PiSP support.
The cameras are not synchronized or stereo calibrated.

## Prepare the Pi

Install the ROS driver dependencies in a Pi terminal:

```bash
sudo apt update
sudo apt install ros-jazzy-camera-ros
```

For two IMX219 modules, back up `/boot/firmware/config.txt` and edit it:

```bash
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.before-dual-camera
sudo nano /boot/firmware/config.txt
```

In the active `[all]` section, keep `camera_auto_detect=0` and replace the
single `dtoverlay=imx219` line with:

```ini
dtoverlay=imx219,cam0
dtoverlay=imx219,cam1
```

Reboot when ready (`sudo reboot`). To check both kernel sensor probes:

```bash
cat /sys/class/video4linux/v4l-subdev*/name
```

There should be two IMX219 entries. A `/dev/video*` count is not a camera count.
If only one sensor appears, power off before checking the ribbon connections.

## Build and publish

```bash
source /opt/ros/jazzy/setup.bash
cd /home/vacuum/ros2_ws
./src/vacuum_crawler/vacuum_cameras/scripts/build_vendor.sh
colcon build --packages-select vacuum_cameras
source install/setup.bash
ros2 launch vacuum_cameras cameras.launch.xml
```

Default requested output is 320×240 RGB888 (published as ROS `bgr8`) at
10 FPS, with JPEG quality 60. These defaults reduce CPU and network load when
both cameras run alongside the motor controller. libcamera may adjust the
resolution or frame duration to the supported sensor range.
Check the startup output for the selected mode and camera IDs. To swap feeds:

```bash
ros2 launch vacuum_cameras cameras.launch.xml camera0:=1 camera1:=0
```

The camera arguments are integer libcamera indexes. Check the startup output
to confirm which physical connector corresponds to each feed.

Topics:

| Camera | Image | Camera information |
| --- | --- | --- |
| 0 | `/cameras/camera0/image_raw` | `/cameras/camera0/camera_info` |
| 1 | `/cameras/camera1/image_raw` | `/cameras/camera1/camera_info` |

From another sourced terminal, verify actual messages:

```bash
ros2 topic hz /cameras/camera0/image_raw
ros2 topic hz /cameras/camera1/image_raw
```

Run each command separately; Ctrl+C stops the rate check.

## RViz and a future GUI

Run `rviz2` on a graphical ROS machine. Add two **Image** displays, select the
image topics above, and set Reliability to **Best Effort** if necessary.
For RViz on another PC, prefer compressed transport even over Ethernet:

```bash
# Run on the PC that runs RViz (for ROS Jazzy):
sudo apt install ros-jazzy-compressed-image-transport
```

Restart RViz after installing the plugin. In Jazzy, select the compressed topic
**directly** in each Image display:
`/cameras/camera0/image_raw/compressed` and
`/cameras/camera1/image_raw/compressed`. Under Topic, set **Reliability Policy**
to **Best Effort**, **History Policy** to **Keep Last**, and **Depth** to **1**.
Disable other Image/Camera displays that still subscribe to the raw topics.
The earlier instruction to use a separate Transport Hint field was incorrect for
this Jazzy RViz implementation.

A ready-made preset is in `config/cameras.rviz`. From your PC terminal:

```bash
scp vacuum@10.42.0.187:/home/vacuum/ros2_ws/src/vacuum_crawler/vacuum_cameras/config/cameras.rviz ~/vacuum-cameras.rviz
source /opt/ros/jazzy/setup.bash
rviz2 -d ~/vacuum-cameras.rviz
```

Close the old RViz instance first so its raw subscriptions do not remain active.
The preset contains only the two Image displays and does not need robot TF.
It is installed under `share/vacuum_cameras/config` when the package is built.

### Image freeze investigation (October 28, 2026)

A 12-second measurement on the Pi found both cameras publishing at 10.03 FPS,
with maximum frame gaps around 103 ms. Raw frame timestamp ages had medians of
14–15 ms, and compressed frames 14–16 ms. The Ethernet link reported 1 Gbps,
no interface errors/drops, and a five-packet ping to the PC averaged 0.5 ms.
These are point-in-time measurements, not proof against intermittent faults.

RViz was subscribing to both **raw** topics with **reliable** delivery. Raw
payload is approximately 37 Mbps combined. The measured JPEG payload averaged
about 0.68 Mbps combined for the scenes sampled. The compressed preset substantially
reduces transport load and avoids requesting retransmission of old preview frames.
This is a mitigation to test, not a confirmed diagnosis of the remote freeze.

If the Image display freezes again, leave it running and compare these commands
on the Pi and on the viewing PC (one topic at a time):

```bash
ros2 topic hz /cameras/camera0/image_raw/compressed --qos-reliability best_effort
ros2 topic hz /cameras/camera1/image_raw/compressed --qos-reliability best_effort
```

If the Pi continues at 10 FPS but the PC stalls, investigate DDS/network delivery.
If both receive steadily while the picture in RViz freezes, inspect the Image
Status/message counter and the RViz rendering path. If the Pi also stalls,
investigate capture and camera-driver logs. Each rate-check command adds a
subscriber, so use short checks and stop them afterward.

Two 320×240 `bgr8` feeds at 10 FPS require about 37 Mbps of raw image payload
alone, compared with 442 Mbps at 640×480 and 30 FPS. Compressed transport
reduces network traffic further. JPEG quality defaults to 60 and can be changed
with `jpeg_quality:=50` on launch or the node's `jpeg_quality` parameter at
runtime. JPEG quality only affects compressed subscribers; resolution and frame
rate affect both raw and compressed streams.

The `frame_duration_us` launch argument sets both libcamera `FrameDurationLimits`
to the same duration: FPS = 1,000,000 / duration. For an even lighter 5 FPS preview:

```bash
ros2 launch vacuum_cameras cameras.launch.xml frame_duration_us:=200000 jpeg_quality:=50
```

Restart the camera launch after rebuilding to apply changed defaults. Motor
command and serial timeouts are unchanged; a control-node exit still needs its
error output checked if reducing camera load does not resolve it.

If video freezes, check `ros2 topic hz /cameras/camera0/image_raw` on the Pi
and compare it with the PC's received rate; a healthy Pi rate points to transport
or visualization rather than camera capture.

Use Image displays for a plain preview; Camera displays additionally need
calibration and transforms. No robot TF is required for Image displays.

For RViz on another machine, source ROS on both machines, use the same
`ROS_DOMAIN_ID`, and ensure ROS discovery and traffic can cross the network.
Two uncompressed RGB feeds can consume significant bandwidth; begin at the
default resolution. A future GUI can subscribe to these same
`sensor_msgs/msg/Image` topics. CameraInfo is published alongside images, but
intrinsics remain uncalibrated until camera calibration is supplied.

## Ubuntu 24.04 / kernel 6.8 camera compatibility

On this Pi, both IMX219 sensors probe correctly, but the packaged ROS libcamera
0.7.2 expects mainline CFE device names such as `rp1-cfe-csi2-ch0`. The kernel
exposes Raspberry Pi downstream names such as `rp1-cfe-csi2_ch0`. The result is
`Unable to acquire a CFE instance` followed by `no cameras available`.

The compatible stack lives entirely inside this package:

```text
vacuum_cameras/
  launch/cameras.launch.xml
  patches/camera_ros-std-span.patch
  scripts/build_vendor.sh       # restores and builds pinned dependencies
  scripts/camera_node.in        # selects the private runtime automatically
  scripts/check_streams.py      # checks both cameras together
  vendor/                      # generated, ignored by Git and colcon
    src/                       # libcamera and camera_ros sources
    tools/                     # isolated Python build tools
    build/
    install/                   # private libraries and driver
```

The build script pins Raspberry Pi libcamera to
`c682979b50695b93526adb1e66db33743484b63b` and camera_ros to
`f4023dc09cfc5fd36fedbb3656ad3a98733e297a`. It applies the included C++20
`std::span` compatibility patch. libcamera also builds libpisp 1.5.0 locally
when it is not found through pkg-config. Build tools require Python venv/pip, Git,
CMake, a C++ compiler, and the development dependencies for libcamera and
camera_ros. The ROS camera package supplies the ROS dependencies on this Pi.
The first build needs internet access and takes several minutes. Repeat builds
reuse the generated files; `CAMERA_BUILD_JOBS` controls parallel compilation
(default 2).

The launch file runs the `vacuum_cameras` wrapper, which selects the private
libraries, IPA modules and tuning files for its camera process. No extra overlay
needs sourcing. Ordinary colcon build/install/log outputs remain in the usual
workspace directories. The wrapper references this source package's vendor
folder: retain it, and rebuild after moving the package. If vendor is deleted,
rerun `scripts/build_vendor.sh` and rebuild the package.

After building, daily use is:

```bash
source /opt/ros/jazzy/setup.bash
source /home/vacuum/ros2_ws/install/setup.bash
ros2 launch vacuum_cameras cameras.launch.xml
```

While the launch is running, check both feeds in another sourced terminal:

```bash
ros2 run vacuum_cameras check_streams.py
```

This requires 60 valid messages from each raw image, JPEG, and CameraInfo topic
within 20 seconds and exits nonzero if any stream fails. CameraInfo can still be
uncalibrated; this checks delivery, not calibration quality.

Upstream references:

- https://github.com/raspberrypi/libcamera
- https://github.com/christianrauch/camera_ros#build-instructions
- https://lists.libcamera.org/pipermail/libcamera-devel/2026-June/059105.html
