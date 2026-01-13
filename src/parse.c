// Extract vid/pid from win32 io path
#include <stdio.h>
#include <stdint.h>
#include <wctype.h>
#include <wchar.h>
#include "../libwpd.h"

__declspec(dllexport)
int wpd_parse_io_path(const wchar_t *path, struct LibWPDDescriptor *d) {
	int len = wcslen(path);
	if (len < 30) {
		return -1;
	}
	
	if (wcsncmp(L"\\\\?\\usb", path, 7)) {
		return -1;
	}

	int of = 7;

	if (path[of] != L'#') {
		return -2;
	}
	of++;

	uint32_t pid = 0;
	uint32_t vid = 0;
	for (int i = 0; i < 2; i++) {
		if (path[of] == L'&') of++;
	
		if (i == 0) {
			if (wcsncmp(L"vid_", &path[of], 4)) {
				return -1;
			}
		} else if (i == 1) {
			if (wcsncmp(L"pid_", &path[of], 4)) {
				return -1;
			}
		}
		of += 4;

		uint32_t id = 0;

		while (1) {
			char c = (char)path[of];
			if (c >= '0' && c <= '9') {
				id *= 16;
				id += c - '0';
			} else if (c >= 'a' && c <= 'z') {
				id *= 16;
				id += c - 'a' + 10;
			} else {
				break;
			}
			of++;
		}

		if (i == 0) {
			d->vendor_id = id;
		} else if (i == 1) {
			d->product_id = id;
		}
		if (path[of] == L'#') {
			break;
		}
	}

	return 0;
}
