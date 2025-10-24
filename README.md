# YusiManager

- Organize and manage SSH profiles.
- Securely connect to SSH servers using your FIDO2 hardware key.
- Supports sshfs and external file managers such as Krusader and Dolphin.
- Built with Qt 6 and C++20.

---

<img width="1723" height="1003" alt="Screenshot" src="https://github.com/user-attachments/assets/a8a11d58-cf81-4d0a-a51c-4addb1f8bf36" />

---

## Build

Minimums: **CMake ≥ 3.16**, **C++20**, **Qt 6 (Widgets, Network, Svg)**

---

## Quick start
```bash
# Clone the repository
git clone https://github.com/YusiManager/V1.X.git
cd V1.X

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# run
./build/YusiManager
```

If you installed Qt to a non‑standard path, point CMake at it:
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x.x/gcc_64"
```

---

### Debian / Ubuntu
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake \
  qt6-base-dev qt6-base-dev-tools \
  qt6-tools-dev qt6-tools-dev-tools \
  libqt6svg6-dev \
  krusader

git clone https://github.com/YusiManager/V1.X.git
cd V1.X
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/YusiManager
```

### Arch / Manjaro
```bash
sudo pacman -S --needed cmake base-devel qt6-base qt6-tools qt6-svg krusader

git clone https://github.com/YusiManager/V1.X.git
cd V1.X
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/YusiManager
```

### Fedora
```bash
sudo dnf install -y cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel qt6-qtsvg-devel krusader

git clone https://github.com/YusiManager/V1.X.git
cd V1.X
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/YusiManager
```

### openSUSE (Leap/Tumbleweed)
```bash
sudo zypper install -y cmake gcc-c++ libqt6-qtbase-devel libqt6-qttools-devel libqt6-qtsvg-devel krusader

git clone https://github.com/YusiManager/V1.X.git
cd V1.X
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/YusiManager
```

---

## License
[MIT License](LICENSE)

---

## Acknowledgements
Built on Qt. Thanks to all contributors.






