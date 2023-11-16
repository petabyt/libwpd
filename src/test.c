#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "libwpd.h"

struct WpdStruct backend_wpd;

int main() {
	puts("inited name");
	wpd_init(1, L"Camlib WPD");
	int length = 0;
	wchar_t **devices = wpd_get_devices(&backend_wpd, &length);

	//puts("Got devices\n");

	if (length == 0) return -1;

	for (int i = 0; i < length; i++) {
		wprintf(L"Trying device: %s\n", devices[i]);

		int ret = wpd_open_device(&backend_wpd, devices[i]);
		if (ret) {
			return -1;
		}

		int type = wpd_get_device_type(&backend_wpd);
		printf("Found device of type: %d\n", type);
		if (type == WPD_DEVICE_TYPE_CAMERA) {
			return 0;
		}

		wpd_close_device(&backend_wpd);
	}

	return -1;
}
