# libwpd
POSIX-friendly abstraction over [Windows Portable Devices](https://learn.microsoft.com/en-us/windows/win32/windows-portable-devices). This makes it easier to port libusb software (specifically PTP/USB clients) to Windows.

## How to use
The DLL is written in Win32 C++, but exposes a basic C API. See `test.c` for a basic example of usage.

## Limitations
- OpenSession and CloseSession opcodes are ignored. Windows wants complete control over sessions, so we can't run those opcodes. A fake 0x2001 PTP_RC_OK response is sent back for compatibility.
- Transaction IDs are not handled. I don't think WPD lets you have that information.

## API
See `libwpd.h`.
