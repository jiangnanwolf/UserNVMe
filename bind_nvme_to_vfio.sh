#!/bin/bash

VENDOR_ID="144d"
DEVICE_ID="a80a"

echo "[+] Loading vfio-pci..."
sudo modprobe vfio-pci

# Find PCI address
PCI_ADDR=$(lspci -Dn | grep "${VENDOR_ID}:${DEVICE_ID}" | awk '{print $1}')

if [ -z "$PCI_ADDR" ]; then
    echo "[-] No device found with ID ${VENDOR_ID}:${DEVICE_ID}"
    exit 1
fi

echo "[+] Found device at PCI address: ${PCI_ADDR}"

SYSFS_DEVICE_PATH="/sys/bus/pci/devices/${PCI_ADDR}"

# Unbind from current driver (if any)
if [ -L "${SYSFS_DEVICE_PATH}/driver" ]; then
    echo "[+] Unbinding from existing driver..."
    echo "${PCI_ADDR}" | sudo tee "${SYSFS_DEVICE_PATH}/driver/unbind"
else
    echo "[*] Device is not bound to any driver."
fi

# Register ID with vfio-pci (safe to run multiple times)
echo "[+] Registering ID ${VENDOR_ID}:${DEVICE_ID} with vfio-pci..."
echo "${VENDOR_ID} ${DEVICE_ID}" | sudo tee /sys/bus/pci/drivers/vfio-pci/new_id

# Wait a bit to ensure the kernel processes the new_id
sleep 1

# Bind the device
if [ -e "${SYSFS_DEVICE_PATH}/driver_override" ]; then
    echo "vfio-pci" | sudo tee "${SYSFS_DEVICE_PATH}/driver_override"
    echo "[+] Forcing driver override to vfio-pci..."
fi

echo "[+] Binding to vfio-pci..."
echo "${PCI_ADDR}" | sudo tee /sys/bus/pci/drivers/vfio-pci/bind

echo "[+] Done."
