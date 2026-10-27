#!/usr/bin/env bash
# Recreate the Pi 5 camera stack entirely inside this package.
set -eo pipefail
package_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
vendor="$package_dir/vendor"
source /opt/ros/jazzy/setup.bash
mkdir -p "$vendor/src"
touch "$vendor/COLCON_IGNORE"
fetch() {
    local name="$1" url="$2" tag="$3" revision="$4"
    if [[ ! -d "$vendor/src/$name" ]]; then
        git clone --depth 1 --branch "$tag" "$url" "$vendor/src/$name"
    fi
    [[ "$(git -C "$vendor/src/$name" rev-parse HEAD)" == "$revision" ]] || {
        echo "Unexpected $name revision; refusing to overwrite it." >&2; exit 1;
    }
}
fetch libcamera https://github.com/raspberrypi/libcamera.git v0.7.2+rpt20261009 c682979b50695b93526adb1e66db33743484b63b
fetch camera_ros https://github.com/christianrauch/camera_ros.git 0.7.0 f4023dc09cfc5fd36fedbb3656ad3a98733e297a
patch_file="$package_dir/patches/camera_ros-std-span.patch"
if git -C "$vendor/src/camera_ros" apply --check "$patch_file" 2>/dev/null; then
    git -C "$vendor/src/camera_ros" apply "$patch_file"
else
    git -C "$vendor/src/camera_ros" apply --reverse --check "$patch_file"
fi
python3 -m venv --system-site-packages "$vendor/tools"
"$vendor/tools/bin/pip" install meson==1.12.1 ninja==1.13.2 ply==3.11
export PATH="$vendor/tools/bin:$PATH"
export PKG_CONFIG_PATH="$vendor/install/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="$vendor/install/lib:${LD_LIBRARY_PATH:-}"
meson setup "$vendor/build/libcamera" "$vendor/src/libcamera" \
    --prefix="$vendor/install" --libdir=lib --buildtype=release \
    -Dpipelines=rpi/pisp -Dipas=rpi/pisp -Dcam=enabled \
    -Dgstreamer=disabled -Dpycamera=disabled -Dv4l2=disabled \
    -Dtest=false -Ddocumentation=disabled -Dqcam=disabled
meson compile -C "$vendor/build/libcamera" -j "${CAMERA_BUILD_JOBS:-2}"
meson install -C "$vendor/build/libcamera"
cmake -S "$vendor/src/camera_ros" -B "$vendor/build/camera_ros" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$vendor/install" \
    -DPython3_EXECUTABLE=/usr/bin/python3
cmake --build "$vendor/build/camera_ros" -j "${CAMERA_BUILD_JOBS:-2}"
cmake --install "$vendor/build/camera_ros"
echo "Camera dependencies ready. Build vacuum_cameras with colcon next."
