// WPD HRESULT Error codes: https://learn.microsoft.com/en-us/windows/win32/wpd_sdk/error-constants
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
#include <cstdint>
#include "ptp.h"
#include "main.h"

// Global verbose debugging flag (current thread)
static int verbose = 1;

// Client name (current thread)
static LPCWSTR client_name;

void mylog(const char *format, ...) {
	if (verbose == 0) return;
	printf("libwpd: ");
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

extern "C" __declspec(dllexport)
struct WpdStruct *wpd_new() {
	struct WpdStruct *wpd = (struct WpdStruct *)malloc(sizeof(struct WpdStruct));
	wpd->pDevice = NULL;
	wpd->pDevValues = NULL;
	wpd->spResults = NULL;

	const uint32_t default_size = 10000;

	wpd->in_buffer_pos = 0;
	wpd->in_buffer = (uint8_t *)malloc(default_size);
	wpd->in_buffer_size = default_size;

	wpd->out_buffer_pos = 0;
	wpd->out_buffer = (uint8_t *)malloc(default_size);
	wpd->out_buffer_size = default_size;
	wpd->out_buffer_filled = 0;

	return wpd;
}

extern "C" __declspec(dllexport)
void wpd_close(struct WpdStruct *wpd) {
	free(wpd->in_buffer);
	free(wpd->out_buffer);
	free(wpd);
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
PWSTR *wpd_get_devices(struct WpdStruct *wpd, int *numDevices) {
	IPortableDeviceManager* pPortableDeviceManager = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pPortableDeviceManager));
	if (!SUCCEEDED(hr)) {
		mylog("CoCreateInstance Error %X\n", HRESULT_FROM_WIN32(GetLastError()));
		return NULL;
	}

	DWORD mNumDevices;

	hr = pPortableDeviceManager->GetDevices(NULL, &mNumDevices);
	if (!SUCCEEDED(hr)) {
		mylog("Error getting devices length\n");
		return NULL;
	}
	(*numDevices) = (int)mNumDevices;

	mylog("Detected %d devices.\n", mNumDevices);

	PWSTR *deviceIDs = (PWSTR *)malloc(sizeof(PWSTR) * mNumDevices);

	hr = pPortableDeviceManager->GetDevices(deviceIDs, &mNumDevices);
	if (!SUCCEEDED(hr)) {
		mylog("Error getting devices\n");
		return NULL;
	}

	return deviceIDs;
}

extern "C" __declspec(dllexport)
void wpd_free_devices(struct WpdStruct *wpd, PWSTR *devs, int numDevices) {
	for (int i = 0; i < numDevices; i++) {
		CoTaskMemFree(devs[i]);
	}
	free(devs);
}

extern "C" __declspec(dllexport)
int wpd_open_device(struct WpdStruct* wpd, PWSTR deviceId) {
	if (deviceId == NULL) {
		return 1;
	}

	wpd->device_path = _wcsdup(deviceId);

	LPCWSTR wszPnPDeviceID = deviceId;

	HRESULT hr = S_OK;
	IPortableDeviceValues *pClientInformation = NULL;

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
	//HANDLE handle = CreateEvent(nullptr, false, false, nullptr);

	mylog("Opened device %ls\n", wszPnPDeviceID);

	return 0;
}

extern "C" __declspec(dllexport)
int wpd_close_device(struct WpdStruct* wpd) {
	HRESULT hr = 0;
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
int wpd_check_connected(struct WpdStruct *wpd) {
	HANDLE h = CreateFileW(wpd->device_path,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (h == INVALID_HANDLE_VALUE) {
		return -1;
	}
	return 0;
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
		mylog("Completion command failed: %X\n", hr);
		return -1;
	}

	IPortableDevicePropVariantCollection *params;
	hr = wpd->spResults->GetIPortableDevicePropVariantCollectionValue(WPD_PROPERTY_MTP_EXT_RESPONSE_PARAMS, &params);
	if (FAILED(hr)) {
		mylog("Failed to get response params\n");
	} else {
		DWORD params_count;
		params->GetCount(&params_count);

		PROPVARIANT prop;
		cmd->param_length = (int)params_count;
		for (DWORD i = 0; i < params_count; i++) {
			params->GetAt(i, &prop);
			cmd->params[i] = prop.ulVal;
			PropVariantClear(&prop);
		}
	}

	DWORD dwResponseCode = 0;
	hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_RESPONSE_CODE, &dwResponseCode);
	if (FAILED(hr)) {
		mylog("Failed to get response code %X\n", hr);
		dwResponseCode = 0x2000;
	}
	mylog("Return code: 0x%x\n", dwResponseCode);
	cmd->code = (int)dwResponseCode;
	return 0;
}

extern "C" __declspec(dllexport)
uint32_t wpd_get_optimal_data_size(struct WpdStruct *wpd) {
	ULONG cbOptimalDataSize = 0;
	HRESULT hr = wpd->spResults->GetUnsignedIntegerValue(WPD_PROPERTY_MTP_EXT_TRANSFER_TOTAL_DATA_SIZE,
		&cbOptimalDataSize);
	if (FAILED(hr)) {
		mylog("Failed to get WPD_PROPERTY_MTP_EXT_TRANSFER_TOTAL_DATA_SIZE\n");
	}
	mylog("Optimal data size: %d\n", cbOptimalDataSize);
	return cbOptimalDataSize;
}

extern "C" __declspec(dllexport)
int wpd_receive_do_data(struct WpdStruct* wpd, struct PtpCommand* cmd, BYTE *buffer, int length) {
	LPWSTR pwszContext = NULL;
	wpd->spResults->GetStringValue(WPD_PROPERTY_MTP_EXT_TRANSFER_CONTEXT, &pwszContext);

	HRESULT hr = wpd->pDevValues->Clear();
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

	BYTE *bufferOut = NULL;
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

	hr = send_finished_command(wpd, cmd, pwszContext);
	if (hr) return hr;

	CoTaskMemFree(pwszContext);

	return cbBytesRead;
}

extern "C" __declspec(dllexport)
int wpd_send_do_command(struct WpdStruct* wpd, struct PtpCommand* cmd, uint32_t length) {
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
int wpd_send_do_data(struct WpdStruct* wpd, struct PtpCommand* cmd, BYTE *buffer, uint32_t length) {
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

	hr = send_finished_command(wpd, cmd, pwszContext);
	if (hr) return hr;

	CoTaskMemFree(pwszContext);

	return (int)cbBytesWritten;
}

extern "C" __declspec(dllexport)
int wpd_ptp_cmd_write(struct WpdStruct *wpd, void *data, uint32_t size) {
	mylog("Filling buffer +%d\n", size);
	if (wpd->in_buffer_size - wpd->in_buffer_pos <= size) {
		wpd->in_buffer = (uint8_t *)realloc(wpd->in_buffer, wpd->in_buffer_pos + size);
		wpd->in_buffer_size = wpd->in_buffer_pos + size;
		if (wpd->in_buffer == NULL) {
			mylog("realloc(%d) failed", wpd->in_buffer_pos + size);
			abort();
		}
	}

	memcpy(wpd->in_buffer + wpd->in_buffer_pos, data, size);

	wpd->in_buffer_pos += size;

	// Invalidate
	wpd->out_buffer_filled = 0;
	wpd->out_buffer_pos = 0;

	return (int)size;
}

extern "C" __declspec(dllexport)
int wpd_ptp_cmd_read(struct WpdStruct* wpd, void *data, uint32_t size) {
	mylog("Request to send %d\n", size);
	struct PtpCommand cmd;
	struct PtpBulkContainer *bulk = (struct PtpBulkContainer *)(wpd->in_buffer);

	// If there is filled data left, return that before sending new commands
	if ((wpd->out_buffer_filled - wpd->out_buffer_pos) > 0) {
		goto end;
	}

	if (wpd->in_buffer_pos == 0) {
		mylog("Nothing to do\n");
		return 0;
	}

	if (wpd->in_buffer_pos < 12) {
		mylog("input buffer too small (%d)\n", wpd->in_buffer_pos);
		return -1;
	}

	if (bulk->type != PTP_PACKET_TYPE_COMMAND) {
		mylog("Not command packet\n");
		return -1;
	}

	// WPD won't let us do any session operations - Windows explorer controls that
	if (bulk->code == PTP_OC_OpenSession || bulk->code == PTP_OC_CloseSession) {
		struct PtpBulkContainer *out_bulk = (struct PtpBulkContainer *)(wpd->out_buffer);
		out_bulk->length = 12;
		out_bulk->code = PTP_RC_OK;
		out_bulk->type = PTP_PACKET_TYPE_RESPONSE;
		memset(out_bulk->params, 0, 5 * sizeof(uint32_t));
		wpd->out_buffer_filled = 12;
		goto end;
	}

	cmd.code = bulk->code;
	cmd.param_length = static_cast<int>((bulk->length - 12) / 4);
	for (int i = 0; i < cmd.param_length; i++) {
		cmd.params[i] = bulk->params[i];
	}

	// Check if there is a data packet following this one (data phase)
	int rc;
	if (wpd->in_buffer_pos > bulk->length) {
		uint32_t packet_size = wpd->in_buffer_pos - bulk->length;
		if (packet_size < 12) {
			mylog("data packet too small\n");
		}
		struct PtpBulkContainer *in_cmd = (struct PtpBulkContainer *)(wpd->in_buffer + bulk->length);
		if (in_cmd->type != PTP_PACKET_TYPE_DATA) {
			mylog("not data packet\n");
		}

		// Send command packet with a hint for the payload length
		mylog("wpd_send_do_command, hint %d\n", in_cmd->length - 12);
		rc = wpd_send_do_command(wpd, &cmd, in_cmd->length - 12);
		if (rc) return -5;

		// Send data payload
		// this will fill the cmd struct
		cmd.param_length = 0;
		mylog("wpd_send_do_data %d\n", in_cmd->length - 12);
		rc = wpd_send_do_data(wpd, &cmd, (uint8_t *)in_cmd + 12, in_cmd->length - 12);
		if (rc < 0) return -1;

		struct PtpBulkContainer *out_cmd = (struct PtpBulkContainer *)(wpd->out_buffer);
		if (wpd->out_buffer_size < 12) abort();
		out_cmd->length = 12;
		out_cmd->type = PTP_PACKET_TYPE_RESPONSE;
		out_cmd->code = cmd.code;
		out_cmd->transaction = bulk->transaction + 1; // we fake the transaction ID
		wpd->out_buffer_pos = 0;
		wpd->out_buffer_filled = 12;
	} else {
		// Send a command packet, no I->R data phase
		rc = wpd_receive_do_command(wpd, &cmd);
		if (rc) return -3;

		uint32_t payload_size = wpd_get_optimal_data_size(wpd);
		const int max_packet_minus_payload_size = 12 + 12; // enough space for data and response packet
		if (payload_size > (wpd->out_buffer_size - max_packet_minus_payload_size)) {
			mylog("Resizing to %d\n", payload_size + max_packet_minus_payload_size);
			wpd->out_buffer = (uint8_t *)realloc(wpd->out_buffer, payload_size + max_packet_minus_payload_size);
			wpd->out_buffer_size = payload_size + max_packet_minus_payload_size;
			if (wpd->out_buffer == NULL) abort();
		}

		// Receive the PTP response
		payload_size = wpd_receive_do_data(wpd, &cmd, wpd->out_buffer + 12, wpd->out_buffer_size - 12 - 12);

		// Place data packet and cmd packet in the out buffer
		struct PtpBulkContainer *out_cmd = (struct PtpBulkContainer *)(wpd->out_buffer);
		out_cmd->length = 12 + payload_size;
		out_cmd->type = PTP_PACKET_TYPE_DATA;
		out_cmd->code = cmd.code;
		out_cmd->transaction = bulk->transaction + 1;
		int length = 12 + payload_size;

		// Place the response packet 12 bytes
		struct PtpBulkContainer *resp = (struct PtpBulkContainer *)(wpd->out_buffer + length);
		resp->length = 12;
		resp->type = PTP_PACKET_TYPE_RESPONSE;
		resp->code = cmd.code;
		resp->transaction = bulk->transaction + 1;

		length += 12;
		wpd->out_buffer_pos = 0;
		wpd->out_buffer_filled = length;
	}

	end:;
	wpd->in_buffer_pos = 0;

	if (wpd->out_buffer_pos > wpd->out_buffer_filled) {
		mylog("Illegal send data overflow %d > %d\n", wpd->out_buffer_pos, wpd->out_buffer_filled);
		abort();
	}

	// Truncate response
	if ((wpd->out_buffer_filled - wpd->out_buffer_pos) < size) {
		size = wpd->out_buffer_filled - wpd->out_buffer_pos;
	}

	mylog("Sending %d\n", size);
	memcpy(data, wpd->out_buffer + wpd->out_buffer_pos, size);
	wpd->out_buffer_pos += size;
	return (int)size;
}
