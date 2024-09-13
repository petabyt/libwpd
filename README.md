# libwpd
POSIX-friendly abstraction over [Windows Portable Devices](https://learn.microsoft.com/en-us/windows/win32/windows-portable-devices). This makes it easier to port libusb software (specifically PTP/USB clients) to Windows.

## How to use
The DLL is written in Win32 C++, but exposes a basic C API. See `test.c` for a basic example of usage.

## Limitations
- OpenSession and CloseSession opcodes are ignored. a 0x2001 PTP_RC_OK response is sent back.
- Transaction IDs are not handled. I don't think WPD lets you have that information.

## API
See `libwpd.h`. My intent is to always keep the API backwards-compatible.
