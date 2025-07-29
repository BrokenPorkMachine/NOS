#!/bin/bash
set -e

# 1. Install dependencies
echo "[*] Installing EDK2/OVMF dependencies..."
sudo apt update
sudo apt install -y git python3 python3-pip python3-distutils python3-venv \
    build-essential uuid-dev iasl nasm gcc make

# 2. Create workspace
echo "[*] Cloning edk2 repository..."
git clone https://github.com/tianocore/edk2.git || true
cd edk2

# 3. Set up EDK2 build environment (in-tree python venv for tools)
echo "[*] Setting up EDK2 python build environment..."
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install -r pip-requirements.txt

# 4. Build BaseTools
make -C BaseTools

# 5. Source EDK2 env
source edksetup.sh

# 6. Build OVMF (release)
echo "[*] Building OVMF (x86_64, RELEASE)..."
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc

# 7. Find and copy built firmware
echo "[*] Copying OVMF firmware images to ../OVMF/ ..."
cd ..
mkdir -p OVMF
cp edk2/Build/OvmfX64/RELEASE_GCC5/FV/OVMF_CODE.fd OVMF/
cp edk2/Build/OvmfX64/RELEASE_GCC5/FV/OVMF_VARS.fd OVMF/

echo
echo "[*] OVMF build complete!"
echo "    Use with: qemu-system-x86_64 -bios ./OVMF/OVMF_CODE.fd ..."
ls -lh OVMF/
