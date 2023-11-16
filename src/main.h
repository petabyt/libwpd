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
	int data_length;
};

struct WpdStruct {
	IPortableDevice* pDevice;
	IPortableDeviceValues* spResults;
	IPortableDeviceValues* pDevValues;
};

#pragma pack(pop)
#endif
