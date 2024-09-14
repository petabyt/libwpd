#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include "libwpd.h"

int try_commands(struct WpdStruct *wpd) {
	int rc;
	struct LibWPDPtpCommand cmd;
	char buffer[4096];

	cmd.code = 0x1001;
	cmd.param_length = 1;
	cmd.params[0] = 1;
	rc = wpd_receive_do_command(wpd, &cmd);
	printf("Return code: %d\n", rc);

	rc = wpd_receive_do_data(wpd, &cmd, buffer, sizeof(buffer));
	printf("Return code: %d\n", rc);

	return 0;
}

struct PtpBulkContainer {
	uint32_t length;
	uint16_t type;
	uint16_t code;
	uint32_t transaction;
	uint32_t params[5];
};

int try_raw(struct WpdStruct *wpd) {
	unsigned char buffer[512];
	struct PtpBulkContainer cmd;

	// OpenSession
	cmd.length = 12 + 4;
	cmd.type = 1;
	cmd.code = 0x1002;
	cmd.transaction = 1;
	cmd.params[0] = 1;

	int rc = wpd_ptp_cmd_write(wpd, &cmd, 12);
	if (rc < 0) return rc;
	rc = wpd_ptp_cmd_read(wpd, buffer, 512);
	if (rc < 0) return rc;

	// GetDeviceInfo
	cmd.length = 12;
	cmd.type = 1;
	cmd.code = 0x1001;
	cmd.transaction = 1;

	rc = wpd_ptp_cmd_write(wpd, &cmd, 12);
	if (rc < 0) return rc;
	rc = wpd_ptp_cmd_read(wpd, buffer, 512);
	if (rc < 0) return rc;

	for (int i = 0; i < rc; i++) {
		printf("%02x ", buffer[i]);
	}
	return 0;
}

int connect() {
	struct WpdStruct *wpd = wpd_new();

	puts("----------------- Getting devices ---------------");
	int length = 0;
	wchar_t **devices = wpd_get_devices(wpd, &length);

	if (length == 0) {
		puts("No devices found");
		return -1;
	}

	for (int i = 0; i < length; i++) {
		wprintf(L"Trying device: %s\n", devices[i]);

		int ret = wpd_open_device(wpd, devices[i]);
		if (ret) {
			return -1;
		}

		int type = wpd_get_device_type(wpd);
		printf("Found device of type: %d\n", type);
		if (type == WPD_DEVICE_TYPE_CAMERA) {
			printf("Trying commands\n");
			try_raw(wpd);
			printf("Done\n");
		}

		wpd_close_device(wpd);
	}

	return 0;	
}

int main() {
	puts("Starting test");
	wpd_init(1, L"Camlib WPD");

	connect();

	return -1;
}
