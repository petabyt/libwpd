#include <windows.h>
#include <conio.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string>
#include <initguid.h>
#include <PortableDeviceApi.hpp>
#include <PortableDevice.hpp>
#include <WpdMtpExtensions.hpp>
#include <initguid.h>
#include <wpd_fills.hpp>
#include <codecvt>
#include <locale>

#include "main.h"

// Global verbose debugging flag (current thread)
static int verbose = 1;

// Client name (current thread)
static LPCWSTR client_name;

static void mylog(const char* format, ...) {
	if (verbose == 0) return;
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

extern "C" __declspec(dllexport)
int wpd_init(int v, LPCWSTR name) {
	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (hr != S_OK) {
		mylog("Couldn't coinitialize hr=%d err=%d\n", hr, GetLastError());
		return 1;
	}

	verbose = v;
	client_name = name;

	return 0;
}

extern "C" __declspec(dllexport)
PWSTR * wpd_get_devices(struct WpdStruct* wpd, DWORD * numDevices) {
	wpd->pDevice = NULL;
	wpd->pDevValues = NULL;
	wpd->spResults = NULL;

	IPortableDeviceManager* pPortableDeviceManager = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pPortableDeviceManager));
	if (!SUCCEEDED(hr)) {
		mylog("CoCreateInstance Error %X\n", HRESULT_FROM_WIN32(GetLastError()));
		return NULL;
	}

	hr = pPortableDeviceManager->GetDevices(NULL, numDevices);
	if (!SUCCEEDED(hr)) {
		mylog("Error getting devices length\n");
		return NULL;
	}

	mylog("Detected %d devices.\n", (*numDevices));

	PWSTR* deviceIDs = (PWSTR*)malloc(sizeof(PWSTR) * (*numDevices));

	hr = pPortableDeviceManager->GetDevices(deviceIDs, numDevices);
	if (!SUCCEEDED(hr)) {
		mylog("Error getting devices\n");
		return NULL;
	}

	return deviceIDs;
}

extern "C" __declspec(dllexport)
int wpd_open_device(struct WpdStruct* wpd, PWSTR deviceId) {
	if (deviceId == NULL) {
		return 1;
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
		mylog("Failed to create portable devices");
		return hr;
	}

	if (FAILED(pClientInformation->SetStringValue(WPD_CLIENT_NAME, client_name))) {
		mylog("Failed to set WPD_CLIENT_NAME: %d\n", GetLastError());
		return 1;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, CLIENT_MAJOR_VER)))
	{
		mylog("Failed to set WPD_CLIENT_MAJOR_VERSION: %d\n", GetLastError());
		return 1;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, CLIENT_MINOR_VER)))
	{
		mylog("Failed to set WPD_CLIENT_MINOR_VERSION: %d\n", GetLastError());
		return 1;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, CLIENT_REVISION)))
	{
		mylog("Failed to set WPD_CLIENT_REVISION: %d\n", GetLastError());
		return 1;
	}

	if (FAILED(pClientInformation->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION))) {
		mylog("Failed to set WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE: %d\n", GetLastError());
		return 1;
	}

	hr = CoCreateInstance(
		CLSID_PortableDeviceFTM,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IPortableDevice,
		(VOID**)&wpd->pDevice
	);
	if (hr) {
		mylog("Failed to create portable device\n");
		return 1;
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
		mylog("Failed to open device %X\n", hr);
		return 1;
	}

	wpd->pClientInformation = pClientInformation;

	// TODO:
	HANDLE handle = CreateEvent(nullptr, false, false, nullptr);

	mylog("Opened device %ls\n", wszPnPDeviceID);

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_close_device(struct WpdStruct* wpd) {
	HRESULT hr;
	if (wpd->pDevice != NULL) hr = wpd->pDevice->Close();
	if (wpd->pDevValues != NULL) hr |= wpd->pDevValues->Clear();
	if (wpd->spResults != NULL) hr |= wpd->spResults->Clear();

	if (FAILED(hr)) {
		mylog("Failed to close the device");
	}

	return hr;
}

extern "C" __declspec(dllexport)
int wpd_get_device_type(struct WpdStruct* wpd) {
	IPortableDeviceContent* content;
	HRESULT hr = wpd->pDevice->Content(&content);
	if (hr) { mylog("content fail\n"); return -1; }

	IPortableDeviceProperties* properties;
	hr = content->Properties(&properties);
	if (hr) { mylog("properties fail (%d)\n", hr); return -1; }

	IPortableDeviceKeyCollection* keys = NULL;
	IPortableDeviceValues* values = NULL;
	properties->GetValues(WPD_DEVICE_OBJECT_ID, keys, &values);
	
	ULONG ti = 0;
	hr = values->GetUnsignedIntegerValue(WPD_DEVICE_TYPE, &ti);
	if (hr) {
		mylog("WPD_DEVICE_TYPE not found\n");
		return -1;
	}
	mylog("Device type: %d\n", ti);

	values->Release();
	properties->Release();
	content->Release();

	return (int)ti;
}

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
		mylog("Error setting command info\n");
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
		mylog("Error creating prop variant collection\n");
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
		mylog("Set parameters\n");
	}
	else {
		mylog("Error setting parameters\n");
		return hr;
	}

	spMtpParams->Release();

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_receive_do_command(struct WpdStruct* wpd, struct PtpCommand* cmd) {
	wpd->spResults = NULL;
	HRESULT hr = wpd_prep_command(
		wpd,
		WPD_COMMAND_MTP_EXT_EXECUTE_COMMAND_WITH_DATA_TO_READ,
		cmd
	);

	hr = wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (FAILED(hr)) {
		mylog("Failed to send command.\n");
		return -1;
	}

	return 0;
}

int send_finished_command(struct WpdStruct* wpd, struct PtpCommand* cmd, LPWSTR pwszContext) {
	// Send the required "completion" command
	wpd->spResults->Release();
	wpd->pDevValues->Clear();

	HRESULT hr = wpd->pDevValues->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY,
		WPD_COMMAND_MTP_EXT_END_DATA_TRANSFER.fmtid);
	hr |= wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID,
		WPD_COMMAND_MTP_EXT_END_DATA_TRANSFER.pid);
	hr |= wpd->pDevValues->SetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, pwszContext);

	hr |= wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (SUCCEEDED(hr)) {
		mylog("Sent the completion command.\n");
	} else {
		mylog("Failed to send the completion command\n");
		return -1;
	}

	wpd->spResults->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &hr);
	if (FAILED(hr)) {
		mylog("Completion command failed\n");
		return -1;
	}

	DWORD dwResponseCode = 0;
	hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_RESPONSE_CODE, &dwResponseCode);
	if (FAILED(hr)) {
		mylog("Failed to get response code %X\n", hr);
		dwResponseCode = 0x2001;
	}
	mylog("Return code: %X\n", dwResponseCode);
	cmd->code = dwResponseCode;
	return 0;
}

extern "C" __declspec(dllexport)
int wpd_receive_do_data(struct WpdStruct* wpd, struct PtpCommand* cmd, BYTE * buffer, int length) {
	ULONG cbOptimalDataSize = 0;
	HRESULT hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_TOTAL_DATA_SIZE,
		&cbOptimalDataSize);

	mylog("Optimal data size: %d, max %d\n", cbOptimalDataSize, length);

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
		mylog("Failed to receive data");
		return -1;
	}

	wpd->spResults->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &hr);
	if (hr) {
		mylog("Failed to finish command (%X)\n", hr);
		return -1;
	}

	BYTE* bufferOut = NULL;
	ULONG cbBytesRead = 0;
	hr = wpd->spResults->GetBufferValue(WPD_PROPERTY_MTP_EXT_TRANSFER_DATA, &bufferOut, &cbBytesRead);
	if (hr) {
		mylog("Failed to get data\n");
		return -1;
	}

	mylog("Read %d bytes\n", cbBytesRead);

	// Win32 quirk note: the data is not actually transferred into `buffer`. Makes no sense, but Windows
	// Requires it for some reason. We'll just copy it to where it should be.
	memcpy(buffer, bufferOut, cbBytesRead);

	//IPortableDevicePropVariantCollection *params;
	//wpd->spResults->GetIPortableDevicePropVariantCollectionValue(WPD_PROPERTY_MTP_EXT_RESPONSE_PARAMS, &params);

	send_finished_command(wpd, cmd, pwszContext);

	CoTaskMemFree(pwszContext);

	return cbBytesRead;
}

extern "C" __declspec(dllexport)
int wpd_send_do_command(struct WpdStruct* wpd, struct PtpCommand* cmd, int length) {
	wpd->spResults = NULL;
	HRESULT hr = wpd_prep_command(
		wpd,
		WPD_COMMAND_MTP_EXT_EXECUTE_COMMAND_WITH_DATA_TO_WRITE,
		cmd
	);
	if (FAILED(hr)) {
		mylog("Failed to prep command\n");
		return -1;
	}

	hr = wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_TOTAL_DATA_SIZE, length);
	if (FAILED(hr)) {
		mylog("Failed to set data size\n");
		return -1;
	}

	hr = wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (FAILED(hr)) {
		mylog("Failed to send command\n");
		return -1;
	}

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_send_do_data(struct WpdStruct* wpd, struct PtpCommand* cmd, BYTE *buffer, int length) {
	LPWSTR pwszContext = NULL;
	wpd->spResults->GetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, &pwszContext);

	wpd->pDevValues->Clear();

	HRESULT hr = wpd->pDevValues->SetGuidValue(WPD_PROPERTY_COMMON_COMMAND_CATEGORY,
		WPD_COMMAND_MTP_EXT_WRITE_DATA.fmtid);
	if (FAILED(hr)) { mylog("FAILED X\n"); return -1; }
	hr = wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_COMMON_COMMAND_ID,
			WPD_COMMAND_MTP_EXT_WRITE_DATA.pid);
	if (FAILED(hr)) { mylog("FAILED X\n"); return -1; }
	hr = wpd->pDevValues->SetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, pwszContext);
	if (FAILED(hr)) { mylog("FAILED X\n"); return -1; }
	hr = wpd->pDevValues->SetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_NUM_BYTES_TO_WRITE, length);
	if (FAILED(hr)) { mylog("FAILED X\n"); return -1; }
	hr = wpd->pDevValues->SetBufferValue(WPD_PROPERTY_MTP_EXT_TRANSFER_DATA, buffer, length);
	if (FAILED(hr)) {
		mylog("Failed to set data properties\n");
		return -1;
	}

	hr = wpd->pDevice->SendCommand(0, wpd->pDevValues, &wpd->spResults);
	if (SUCCEEDED(hr)) {
		hr = wpd->spResults->GetErrorValue(WPD_PROPERTY_COMMON_HRESULT, &hr);
		if (SUCCEEDED(hr)) {
			mylog("Successfully sent data\n");
		} else {
			mylog("Failed to send data\n");
			return -1;
		}
	} else {
		mylog("Failed to send data\n");
		return -1;
	}

	DWORD cbBytesWritten = 0;
	hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_NUM_BYTES_WRITTEN,
		&cbBytesWritten);

	mylog("Sent %d bytes\n", cbBytesWritten);

	send_finished_command(wpd, cmd, pwszContext);

	CoTaskMemFree(pwszContext);

	return cbBytesWritten;
}
