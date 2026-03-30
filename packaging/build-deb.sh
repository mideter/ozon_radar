#!/usr/bin/env bash
# Сборка минимального .deb: бинарник в /usr/lib/ozon/bin, скрипты в /usr/lib/ozon/scripts,
# symlink /usr/bin/ozon_cpp (так находится ozon_fetch.py относительно каталога exe).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build-release"
STAGE="${ROOT}/packaging/stage"
PKG_NAME="ozon-cpp"

VERSION="${VERSION:-$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo 0.1.0)}"
ARCH="${ARCH:-$(dpkg-architecture -qDEB_HOST_ARCH 2>/dev/null || uname -m)}"
# Нормализуем имя архитектуры для deb (amd64, arm64, …)
case "$ARCH" in
  x86_64) ARCH=amd64 ;;
  aarch64) ARCH=arm64 ;;
esac

echo "==> CMake (Release) in $BUILD"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

echo "==> Staging package"
rm -rf "$STAGE"
mkdir -p "$STAGE/DEBIAN" \
  "$STAGE/usr/lib/ozon/bin" \
  "$STAGE/usr/lib/ozon/scripts" \
  "$STAGE/usr/bin"

install -m755 "$BUILD/ozon_cpp" "$STAGE/usr/lib/ozon/bin/ozon_cpp"
for f in "$ROOT"/scripts/*.py; do
  install -m644 "$f" "$STAGE/usr/lib/ozon/scripts/$(basename "$f")"
done
ln -sf ../lib/ozon/bin/ozon_cpp "$STAGE/usr/bin/ozon_cpp"

cat >"$STAGE/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: Unmaintained snapshot <root@localhost>
Depends: python3, python3-undetected-chromedriver, chromium | chromium-browser, chromium-driver, libqt5core5a, libqt5gui5, libqt5widgets5, libqt5qml5, libqt5quick5, libqt5quickcontrols2-5, qml-module-qtquick2, qml-module-qtquick-controls2, qml-module-qtquick-layouts, qml-module-qtquick-window2
Description: Ozon Scraper — Qt UI и Python-загрузчик страниц
 Парсер витрины Ozon (Qt Quick + Python/Chrome).
EOF

DEB_OUT="${ROOT}/${PKG_NAME}_${VERSION}_${ARCH}.deb"
echo "==> Building $DEB_OUT"
dpkg-deb --root-owner-group --build "$STAGE" "$DEB_OUT"
echo "Done: $DEB_OUT"
ls -la "$DEB_OUT"
