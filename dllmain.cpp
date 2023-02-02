#include "pch.h"
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string>

#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <WpdMtpExtensions.h>
#pragma comment(lib,"PortableDeviceGUIDs.lib")

#include <codecvt>
#include <locale>

struct PtpCommand {
	int code;
	uint32_t params[5];
	int param_length;
	int data_length;
};

LPCWSTR client_name;
#define CLIENT_MAJOR_VER	1
#define CLIENT_MINOR_VER	0
#define CLIENT_REVISION		0
#define MAX_DEVICES 10
int verbose = 1;

#define LOG(...) if (verbose) printf(__VA_ARGS__)

struct WpdStruct {
	IPortableDevice* pDevice;
	LPWSTR last_context_cookie;
	IPortableDeviceValues* spResults;
	IPortableDeviceValues* pDevValues;
};

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport)
int wpd_init(int v, LPCWSTR name) {
	if (CoInitialize(nullptr)) {
		LOG("Couldn't coinitialize\n");
		return 1;
	}

	verbose = v;
	client_name = name;

	return 0;
}

extern "C" __declspec(dllexport)
PWSTR * wpd_get_devices(struct WpdStruct* wpd, DWORD * numDevices) {
	IPortableDeviceManager* pPortableDeviceManager = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pPortableDeviceManager));
	if (!SUCCEEDED(hr)) {
		LOG("Error %d\n", GetLastError());
		return NULL;
	}

	hr = pPortableDeviceManager->GetDevices(NULL, numDevices);
	if (!SUCCEEDED(hr)) {
		LOG("Error getting devices length\n");
		return NULL;
	}

	LOG("Detected %d devices.\n", *numDevices);

	PWSTR* deviceIDs = (PWSTR*)malloc(sizeof(PWSTR) * *numDevices);

	hr = pPortableDeviceManager->GetDevices(deviceIDs, numDevices);
	if (!SUCCEEDED(hr)) {
		LOG("Error getting devices\n");
		return NULL;
	}

	return deviceIDs;
}

extern "C" __declspec(dllexport)
int wpd_open_device(struct WpdStruct* wpd, PWSTR deviceId) {
	if (deviceId == NULL) {
		return NULL;
	}

	LPCWSTR wszPnPDeviceID = deviceId;

	HRESULT hr = S_OK;
	IPortableDeviceValues* pClientInformation = NULL;

	HRESULT ClientInfoHR = S_OK;

	hr = CoCreateInstance(
		CLSID_PortableDeviceValues,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IPortableDeviceValues,
		(VOID**)&pClientInformation
	);
	if (hr) {
		LOG("Failed to create portable devices");
		return hr;
	}

	if (FAILED(pClientInformation->SetStringValue(WPD_CLIENT_NAME, client_name))) {
		LOG("Failed to set WPD_CLIENT_NAME: %d\n", GetLastError());
		return hr;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, CLIENT_MAJOR_VER)))
	{
		LOG("Failed to set WPD_CLIENT_MAJOR_VERSION: %d\n", GetLastError());
		return hr;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, CLIENT_MINOR_VER)))
	{
		LOG("Failed to set WPD_CLIENT_MINOR_VERSION: %d\n", GetLastError());
		return hr;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, CLIENT_REVISION)))
	{
		LOG("Failed to set WPD_CLIENT_REVISION: %d\n", GetLastError());
		return hr;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION))) {
		LOG("Failed to set WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE: %d\n", GetLastError());
		return hr;
	}

	hr = CoCreateInstance(
		CLSID_PortableDeviceFTM,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IPortableDevice,
		(VOID**)&wpd->pDevice
	);
	if (hr) {
		LOG("Failed to create portable device\n");
		return hr;
	}

	hr = wpd->pDevice->Open(wszPnPDeviceID, pClientInformation);
	if (hr == E_ACCESSDENIED) {
		// Attempt to open for read-only access
		pClientInformation->SetUnsignedIntegerValue(
			WPD_CLIENT_DESIRED_ACCESS,
			GENERIC_READ);
		hr = wpd->pDevice->Open(wszPnPDeviceID, pClientInformation);
	}

	if (!SUCCEEDED(hr)) {
		LOG("Failed to open device %X\n", hr);
		return hr;
	}

	// Get properties
#if 0
	// Release the IPortableDeviceValues that contains the client information when finished
	if (pClientInformation != NULL) {
		pClientInformation->Release();
		pClientInformation = NULL;
	}
#endif

	// TODO:
	HANDLE handle = CreateEvent(nullptr, false, false, nullptr);

	LOG("Opened device %ls\n", wszPnPDeviceID);

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_get_device_type(struct WpdStruct* wpd) {
	IPortableDeviceContent* content;
	HRESULT hr = wpd->pDevice->Content(&content);
	if (hr) { LOG("content fail\n"); return -1; }
	IPortableDeviceProperties* properties;
	hr = content->Properties(&properties);
	if (hr) { LOG("properties fail\n"); return -1; }
	IPortableDeviceKeyCollection* keys = NULL;
	IPortableDeviceValues* values = NULL;
	hr = properties->GetValues(WPD_DEVICE_OBJECT_ID, keys, &values);
	if (hr) { LOG("getvalues fail\n"); return -1; }
	ULONG ti = 0;
	values->GetUnsignedIntegerValue(WPD_DEVICE_TYPE, &ti);
	LOG("Device type: %d\n", ti);
	return (int)ti;
}

//int wpd_send_command_data(char* buffer, int code, int* params);
//int wpd_get_command_data(char* buffer, int code, int* params);
//int wpd_get_command_data(char* buffer, int code, int* params);

extern "C" __declspec(dllexport)
int wpd_prep_command(struct WpdStruct* wpd, PROPERTYKEY type, struct PtpCommand* cmd) {
	wpd->pDevValues = nullptr;
	HRESULT hr = CoCreateInstance(
		CLSID_PortableDeviceValues,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&wpd->pDevValues)
	);

	hr |= wpd->pDevValues->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY, type.fmtid);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID, type.pid);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_OPERATION_CODE, (ULONG)cmd->code);
	if (hr) {
		LOG("Error setting command info\n");
		return hr;
	}

	IPortableDevicePropVariantCollection* spMtpParams = nullptr;
	hr = CoCreateInstance(
		CLSID_PortableDevicePropVariantCollection,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&spMtpParams)
	);
	if (hr) {
		LOG("Error creating prop variant collection\n");
		return hr;
	}

	for (int i = 0; i < cmd->param_length; i++) {
		PROPVARIANT param = { 0 };
		param.vt = VT_UI4;
		param.ulVal = cmd->params[i];
		spMtpParams->Add(&param);
	}

	hr = wpd->pDevValues->SetIPortableDevicePropVariantCollectionValue(
		WPD_PROPERTY_MTP_EXT_OPERATION_PARAMS, spMtpParams);
	if (SUCCEEDED(hr)) {
		LOG("Set parameters\n");
	}
	else {
		LOG("Error setting parameters\n");
		return hr;
	}

	spMtpParams->Release();

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_recieve_do_command(struct WpdStruct* wpd, struct PtpCommand* cmd) {
	wpd->spResults = NULL;
	HRESULT hr = wpd_prep_command(
		wpd,
		WPD_COMMAND_MTP_EXT_EXECUTE_COMMAND_WITH_DATA_TO_READ,
		cmd
	);

	hr = wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);

	if (SUCCEEDED(hr)) {
		LOG("Success send\n");
		wpd->spResults->GetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, &wpd->last_context_cookie);
		return 0;
	}
	else {
		return hr;
	}

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_recieve_do_data(struct WpdStruct* wpd, struct PtpCommand* cmd, BYTE * buffer, int length) {
	ULONG cbOptimalDataSize = 0;
	HRESULT hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_TOTAL_DATA_SIZE,
		&cbOptimalDataSize);

	printf("Optimal data size: %d\n", cbOptimalDataSize);

	LPWSTR pwszContext = NULL;
	wpd->spResults->GetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, &pwszContext);

	hr = wpd->pDevValues->Clear();
	hr |= wpd->pDevValues->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY,
		WPD_COMMAND_MTP_EXT_READ_DATA.fmtid);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID,
		WPD_COMMAND_MTP_EXT_READ_DATA.pid);
	hr |= wpd->pDevValues->SetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, pwszContext);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_NUM_BYTES_TO_READ, length);
	hr |= wpd->pDevValues->SetBufferValue(WPD_PROPERTY_MTP_EXT_TRANSFER_DATA, buffer, length);

	hr = wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (!SUCCEEDED(hr)) {
		LOG("Failed to recieve data");
		return -1;
	}

	wpd->spResults->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &hr);
	if (hr) {
		LOG("Failed to finish command\n");
		return -1;
	}

	BYTE* bufferOut = NULL;
	ULONG cbBytesRead = 0;
	hr = wpd->spResults->GetBufferValue(WPD_PROPERTY_MTP_EXT_TRANSFER_DATA, &bufferOut, &cbBytesRead);
	if (hr) {
		LOG("Failed to get data\n");
		return -1;
	}

	// Send the required "completion" command
	wpd->spResults->Release();
	wpd->pDevValues->Clear();
	hr = wpd->pDevValues->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY,
		WPD_COMMAND_MTP_EXT_END_DATA_TRANSFER.fmtid);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID,
		WPD_COMMAND_MTP_EXT_END_DATA_TRANSFER.pid);
	hr |= wpd->pDevValues->SetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, pwszContext);
	hr |= wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (SUCCEEDED(hr)) {
		LOG("Sent the completion command.\n");
	}
	else {
		LOG("Failed to send the completion command\n");
		return -1;
	}

	DWORD dwResponseCode = 0;
	hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_RESPONSE_CODE, &dwResponseCode);
	if (hr) {
		LOG("Failed to get response code %X\n", hr);
		return -1;
	}
	printf("Return code: %X\n", dwResponseCode);
	cmd->code = dwResponseCode;

	//IPortableDevicePropVariantCollection *params;
	//wpd->spResults->GetIPortableDevicePropVariantCollectionValue(WPD_PROPERTY_MTP_EXT_RESPONSE_PARAMS, &params);

	LOG("Read %d bytes\n", cbBytesRead);

	// Win32 quirk note: the data is not actually transferred into `buffer`. Makes no sense, but Windows
	// Requires it for some reason. We'll just copy it to where it should be.
	memcpy(buffer, bufferOut, cbBytesRead);

	return cbBytesRead;
}