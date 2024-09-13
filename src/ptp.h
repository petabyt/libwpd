#define PTP_OC_OpenSession			0x1002
#define PTP_OC_CloseSession			0x1003

#define PTP_RC_OK						0x2001
#define PTP_RC_GeneralError				0x2002

// PTP Packet container types
#define PTP_PACKET_TYPE_COMMAND 	0x1
#define PTP_PACKET_TYPE_DATA		0x2
#define PTP_PACKET_TYPE_RESPONSE	0x3
#define PTP_PACKET_TYPE_EVENT		0x4

#pragma pack(push, 1)

// Standard USB-only packet
struct PtpBulkContainer {
	uint32_t length; // length of packet, in bytes
	uint16_t type; // See PACKET_TYPE_*
	uint16_t code; // See PTP_OC_*
	uint32_t transaction;

	// Parameters are only included in command packets,
	// It is typically considered a part of payload in data packets
	uint32_t params[5];

	// Payload data follows, if any
};

#pragma pack(pop)
