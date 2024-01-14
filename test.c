#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "libwpd.h"

struct WpdStruct backend_wpd;

int connect() {
	puts("----------------- Getting devices ---------------");
	int length = 0;
	wchar_t **devices = wpd_get_devices(&backend_wpd, &length);

	if (length == 0) {
		puts("No devices found");
		return -1;
	}

	for (int i = 0; i < length; i++) {
		wprintf(L"Trying device: %s\n", devices[i]);

		int ret = wpd_open_device(&backend_wpd, devices[i]);
		if (ret) {
			return -1;
		}

		int type = wpd_get_device_type(&backend_wpd);
		printf("Found device of type: %d\n", type);
		if (type == WPD_DEVICE_TYPE_CAMERA) {

			struct LibWPDPtpCommand cmd;
			char buffer[4096];

			cmd.code = 0x1002;
			cmd.param_length = 1;
			cmd.params[0] = 1;
			ret = wpd_receive_do_command(&backend_wpd, &cmd);
			printf("Return code: %d\n", ret);

			ret = wpd_receive_do_data(&backend_wpd, &cmd, buffer, sizeof(buffer));
			printf("Return code: %d\n", ret);

			wpd_close_device(&backend_wpd);

			return 0;
		}

		wpd_close_device(&backend_wpd);
	}

	return 0;	
}

int main() {
	puts("Starting test");
	wpd_init(1, L"Camlib WPD");

	connect();
	connect();
	connect();

	return -1;
}
