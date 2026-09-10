set -euxo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq --no-install-recommends build-essential gcc-12 g++-12 git ca-certificates curl gnupg \
  python3 pkg-config libgl1-mesa-dev xorg-dev libwayland-dev wayland-protocols libxkbcommon-dev libasound2-dev \
  file dpkg-dev fakeroot >/dev/null
curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /usr/share/keyrings/kitware.gpg
echo "deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ jammy main" > /etc/apt/sources.list.d/kitware.list
apt-get update -qq && apt-get install -y -qq --no-install-recommends cmake ninja-build >/dev/null
cmake --version | head -1; g++-12 --version | head -1; ldd --version | head -1
git config --global --add safe.directory /src
CC=gcc-12 CXX=g++-12 cmake -S /src -B /out/b -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUMI_APP_VERSION=0.5.0-rc.4-jammyprobe
cmake --build /out/b -j6
/out/b/desktop/midi-sink --version
ctest --test-dir /out/b --output-on-failure | tail -3
echo "glibc symbols needed:"; objdump -T /out/b/desktop/midi-sink | grep -oE 'GLIBC_[0-9.]+' | sort -Vu | tail -2
echo "PROBE_OK"
