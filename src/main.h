#ifndef LIBWPD_H
#define LIBWPD_H
#pragma pack(push, 1)

#define CLIENT_MAJOR_VER	1
#define CLIENT_MINOR_VER	0
#define CLIENT_REVISION		0
#define MAX_DEVICES			10

struct PtpCommand {
	int code;
	uint32_t params[5];
	int param_length;
};

struct WpdStruct {
	IPortableDevice *pDevice;
	IPortableDeviceValues *spResults;
	IPortableDeviceValues *pDevValues;
	IPortableDeviceValues *pClientInformation;
	PWSTR device_path;

	/// @brief Data coming in from the client
	uint8_t *in_buffer;
	uint32_t in_buffer_size;
	uint32_t in_buffer_pos;

	/// @brief Data coming out of the camera
	uint8_t *out_buffer;
	uint32_t out_buffer_size;
	/// @brief current reading position of out_buffer, incremented as wpd_ptp_cmd_read is called
	uint32_t out_buffer_pos;
	/// @brief Number of bytes of data written to out_buffer by the last transaction.
	uint32_t out_buffer_filled;
};

void mylog(const char* format, ...);

#pragma pack(pop)
#endif
