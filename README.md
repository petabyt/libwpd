# libwpd
POSIX-friendly abstraction over [Windows Portable Devices](https://learn.microsoft.com/en-us/windows/win32/windows-portable-devices). This makes it easier to port libusb software (specifically PTP/USB libraries) to Windows.

## How to use
The DLL is written in Win32 C++, but exposes a basic C API. See `test.c` for a basic example of usage.

The biggest thing to take note of is that WPD doesn't give you a raw IO API. This sends commands to the Windows kernel, requesting to perform
MTP operations. This means that Windows may be performing commands while you are doing stuff. I don't know the exact specifics (I wrote this a while ago)
but just be aware of this. But in my experience, I haven't noticed anything go horribly wrong yet.

## API
See `libwpd.h`. My intent is to always keep the API backwards-compatible.
