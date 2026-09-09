#!/usr/bin/env bash
set -euo pipefail

# Hosted images also configure unrelated browser/vendor repositories. A stale
# Chrome index must not block an audio build, and hash verification stays on.
source /etc/os-release
: "${RUNNER_TEMP:=${TMPDIR:-/tmp}}"
sources="$RUNNER_TEMP/geiger-ubuntu.list"
cat > "$sources" <<EOF
deb http://archive.ubuntu.com/ubuntu ${VERSION_CODENAME} main restricted universe multiverse
deb http://archive.ubuntu.com/ubuntu ${VERSION_CODENAME}-updates main restricted universe multiverse
deb http://security.ubuntu.com/ubuntu ${VERSION_CODENAME}-security main restricted universe multiverse
EOF
apt_options=(-o "Dir::Etc::sourcelist=$sources" -o "Dir::Etc::sourceparts=-" -o "Acquire::Retries=3")
sudo apt-get "${apt_options[@]}" -o APT::Update::Error-Mode=any update -qq
sudo apt-get "${apt_options[@]}" install -y build-essential pkg-config libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev libxrender-dev libxfixes-dev libxi-dev xvfb
sudo apt-get "${apt_options[@]}" install --reinstall -y libxrandr-dev
test -f /usr/include/X11/extensions/Xrandr.h
printf '#include <X11/extensions/Xrandr.h>\n' | c++ -x c++ -fsyntax-only -
