# libmks

This library provides a "Mouse, Keyboard, and Screen" to Qemu using the
D-Bus device support in Qemu and GTK 4.

# Documentation

Nightly documentation can be found [here](https://chergert.pages.gitlab.gnome.org/libmks/libmks1).

# Testing

By default, Qemu will connect to your user session D-Bus if you do not
provide an address for `-display dbus`. Therefore, it is pretty easy to
test things by running Qemu manually and then connecting with the test
program `./tools/mks`.

```sh
qemu-img create -f qcow2 fedora.img 30G
qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    -device virtio-vga-gl,xres=1920,yres=1080 \
    -m 8G \
    -smp 4 \
    -display dbus,gl=on \
    -cdrom Fedora-Workstation.iso \
    -hda fedora.img \
    -boot d
```

and then to run the test widget

```sh
meson setup build
cd build
ninja
./tools/mks
```
