#!/bin/bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
# Development source is not a new release. Never overwrite/reissue an old version.
: "${SYSPULSE_PACKAGE_VERSION:?Assign an explicit reviewed package version before packaging}"
[[ $# == 1 ]] || { echo 'Specify a new explicit output filename'; exit 1; }
[[ "$SYSPULSE_PACKAGE_VERSION" =~ ^[0-9][0-9A-Za-z.+~:-]*$ ]] || { echo 'Invalid package version'; exit 1; }
dpkg --validate-version "$SYSPULSE_PACKAGE_VERSION"
dpkg --compare-versions "$SYSPULSE_PACKAGE_VERSION" gt '3.0.0~native.r11+preview1' || { echo 'Version must exceed the existing R11 package'; exit 1; }
output=$1
output=$(realpath -m -- "$output")
[[ ! -e "$output" && ! -e "$output.sha256" ]] || { echo "Refusing to overwrite an existing artifact"; exit 1; }
[[ $(dpkg --print-architecture) == amd64 ]] || { echo 'amd64 required'; exit 1; }
work=$(mktemp -d); trap 'rm -rf -- "$work"' EXIT
cmake -S "$root" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS='-Wall -Wextra -Wpedantic -Werror' -DCMAKE_EXE_LINKER_FLAGS='-static-libstdc++ -static-libgcc'
cmake --build "$work/build" -j2
ctest --test-dir "$work/build" --output-on-failure
testlist=$(ctest --test-dir "$work/build" -N)
for suite in native-dashboard-x11 native-gui-layout native-diagnostic-learning; do
    grep -Eq "Test +#[0-9]+: ${suite}$" <<< "$testlist" || { echo "Required test missing: $suite"; exit 1; }
done
cp -a "$root/packaging/template" "$work/package"
sed -i "s/^Version:.*/Version: $SYSPULSE_PACKAGE_VERSION/" "$work/package/DEBIAN/control"
mkdir -p "$work/package/usr/bin"
for pair in 'syspulse-dashboard-x11 syspulse-native-preview' 'syspulse-inspect syspulse-native-inspect' 'syspulse-sample syspulse-native-sample' 'syspulse-native-memory-helper syspulse-native-memory-helper' 'syspulse-native-package-helper syspulse-native-package-helper'; do
    read -r bin dest <<< "$pair"
    install -m755 "$work/build/$bin" "$work/package/usr/bin/$dest"
    strip --strip-unneeded "$work/package/usr/bin/$dest"
    ceiling=$(objdump -T "$work/package/usr/bin/$dest" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)
    dpkg --compare-versions "${ceiling#GLIBC_}" le 2.38 || { echo "Incompatible GLIBC symbol $ceiling"; exit 1; }
done
sed -i 's/^Installed-Size:.*/Installed-Size: '"$(du -sk "$work/package/usr" | cut -f1)"'/' "$work/package/DEBIAN/control"
(cd "$work/package"; find usr -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums)
dpkg-deb --root-owner-group --build "$work/package" "$output"
(cd "$(dirname "$output")"; sha256sum "$(basename "$output")" > "$(basename "$output").sha256")
