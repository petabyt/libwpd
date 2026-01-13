// Header for libwpd
#ifndef PTP_WPD_H
#define PTP_WPD_H

#include <stdint.h>
#include <stddef.h>

// From Microsoft
typedef enum tagWPD_DEVICE_TYPES { 
	WPD_DEVICE_TYPE_GENERIC                       = 0,
	WPD_DEVICE_TYPE_CAMERA                        = 1,
	WPD_DEVICE_TYPE_MEDIA_PLAYER                  = 2,
	WPD_DEVICE_TYPE_PHONE                         = 3,
	WPD_DEVICE_TYPE_VIDEO                         = 4,
	WPD_DEVICE_TYPE_PERSONAL_INFORMATION_MANAGER  = 5,
	WPD_DEVICE_TYPE_AUDIO_RECORDER                = 6
}WPD_DEVICE_TYPES;

struct LibWPDDescriptor {
	int product_id;
	int vendor_id;
};

/// @brief Initialize thread
int wpd_init(int verbose, wchar_t *app_name);

// Private, implemented by libwpd
struct WpdStruct {
	void *dev;
	void *results;
	void *values;
};

/// @brief Create new WpdStruct object
struct WpdStruct *wpd_new();

/// @brief Frees all memory in the wpd struct and then frees the wpd struct itself
void wpd_close(struct WpdStruct *wpd);

/// @brief Receive an array of wide strings. Length is set in num_devices. Each string holds the device
/// ID, similar to Linux /dev/bus/usb/001
/// @note free with wpd_free_devices
wchar_t **wpd_get_devices(struct WpdStruct *wpd, int *num_devices);

/// @brief Parse USB vendor ID and product ID from path in list returned by wpd_get_devices
int wpd_parse_io_path(const wchar_t *path, struct LibWPDDescriptor *d);

/// @brief Free list of devices from wpd_get_devices
void wpd_free_devices(struct WpdStruct *wpd, wchar_t **devs, int numDevices);

/// @brief Initialize the WpdStruct with a new connection to the selected device ID wide string
int wpd_open_device(struct WpdStruct *wpd, wchar_t *device_id);

/// @brief Close the device currently connected
int wpd_close_device(struct WpdStruct* wpd);

/// @brief Returns enum WPD_DEVICE_TYPES
int wpd_get_device_type(struct WpdStruct *wpd);

/// @brief Check if the device is attached
/// @returns -1 if device is detached/not found, 0 if attached
int wpd_check_connected(struct WpdStruct *wpd);

/// @brief Generic USB writing command. Will store all sent data into a buffer, which will be processed when wpd_ptp_cmd_read is called.
int wpd_ptp_cmd_write(struct WpdStruct *wpd, void *data, uint32_t size);

/// @brief Processes all PTP packets sent through wpd_ptp_cmd_write, and performs operations. Will then store all results in the 'out' buffer.
/// Every time this function is called, it will return data from the 'out' buffer, as requested by `size`.
int wpd_ptp_cmd_read(struct WpdStruct *wpd, void *data, uint32_t size);

#endif

