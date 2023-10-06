# libwpd
POSIX/mingw friendly abstraction over WPD. This makes it easier to port libusb software to Windows.

The DLL is written in Win32 C++, but exposes a basic C API. See https://github.com/petabyt/camlib/blob/master/src/libwpd.c for a basic implementation.

## API
See `libwpd.h` for a POSIX friendly API.
Note that `wpd_get_devices` returns a pointer to a list of 16-bit Windows-style unicode strings. They will be the standard Win32
format for identifying devices: `\\?\usb#vid_04a9&pid_32b4#5&3a68d27e&0&2#{6ac27878-a6fa-4155-ba85-f98f491d4f33}` Yes, that is the MS
way of something like `/dev/bus/usb/001`.
