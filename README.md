# User-Space NVMe Driver (Learning Project)

This repository is part of my exploration into NVMe internals. It implements a simple user-space NVMe driver, inspired by SPDK, to help better understand the NVMe protocol and low-level storage interactions.

---

## 🚀 Overview

This project demonstrates:

- Unbinding an NVMe SSD from the kernel driver
- Binding it to `vfio-pci` for user-space access
- Communicating with the device using memory-mapped I/O (MMIO)

---

## 🛠 Prerequisites

Before running the code, ensure the following:

- A **secondary NVMe SSD** (do *not* use your boot drive)
- Linux booted from a different device
- Packages installed:

```
sudo apt update
sudo apt install cmake build-essential
```

## ⚙️ Setup

Use the provided script to unbind the NVMe device from the kernel and bind it to vfio-pci:

```
./bind_nvme_to_vfio.sh
```

⚠️ Note: The script contains hardcoded values for the Vendor ID, Device ID, and PCI address.
You must update these values to match your hardware before running the script.

## 💻 Environment
OS: Ubuntu 24.04

Hardware: Lenovo laptop

## 📎 Notes
Code is written in C++

Designed for learning and experimentation purposes

Not production-ready

