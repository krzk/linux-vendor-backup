#ifndef LINUX_MHL_H
#define LINUX_MHL_H

enum {
	MHL_DEVCAP_DEV_STATE,
	MHL_DEVCAP_MHL_VERSION,
	MHL_DEVCAP_DEV_CAT,
	MHL_DEVCAP_ADOPTER_ID_H,
	MHL_DEVCAP_ADOPTER_ID_L,
	MHL_DEVCAP_VID_LINK_MODE,
	MHL_DEVCAP_AUD_LINK_MODE,
	MHL_DEVCAP_VIDEO_TYPE,
	MHL_DEVCAP_LOG_DEV_MAP,
	MHL_DEVCAP_BANDWIDTH,
	MHL_DEVCAP_FEATURE_FLAG,
	MHL_DEVCAP_DEVICE_ID_H,
	MHL_DEVCAP_DEVICE_ID_L,
	MHL_DEVCAP_SCRATCHPAD_SIZE,
	MHL_DEVCAP_INT_STAT_SIZE,
	MHL_DEVCAP_RESERVED,
	MHL_DEVCAP_SIZE
};

#define MHL_DEV_CAT_SINK			0x01
#define MHL_DEV_CAT_SOURCE			0x02
#define MHL_DEV_CAT_POWER			0x10
#define MHL_DEV_CAT_PLIM(x)			((x) << 5)

#define MHL_DEV_VID_LINK_SUPP_RGB444		0x01
#define MHL_DEV_VID_LINK_SUPP_YCBCR444		0x02
#define MHL_DEV_VID_LINK_SUPP_YCBCR422		0x04
#define MHL_DEV_VID_LINK_SUPP_PPIXEL		0x08
#define MHL_DEV_VID_LINK_SUPP_ISLANDS		0x10
#define MHL_DEV_VID_LINK_SUPP_VGA		0x20
#define MHL_DEV_VID_LINK_SUPP_16BPP		0x40

#define MHL_DEV_AUD_LINK_2CH			0x01
#define MHL_DEV_AUD_LINK_8CH			0x02

#define MHL_FEATURE_RCP_SUPPORT			0x01
#define MHL_FEATURE_RAP_SUPPORT			0x02
#define MHL_FEATURE_SP_SUPPORT			0x04
#define MHL_FEATURE_UCP_SEND_SUPPORT		0x08
#define MHL_FEATURE_UCP_RECV_SUPPORT		0x10
#define MHL_FEATURE_RBP_SUPPORT			0x40

#define MHL_VT_GRAPHICS				0x00
#define MHL_VT_PHOTO				0x02
#define MHL_VT_CINEMA				0x04
#define MHL_VT_GAMES				0x08
#define MHL_SUPP_VT				0x80

#define MHL_DEV_LD_DISPLAY			0x01
#define MHL_DEV_LD_VIDEO			0x02
#define MHL_DEV_LD_AUDIO			0x04
#define MHL_DEV_LD_MEDIA			0x08
#define MHL_DEV_LD_TUNER			0x10
#define MHL_DEV_LD_RECORD			0x20
#define MHL_DEV_LD_SPEAKER			0x40
#define MHL_DEV_LD_GUI				0x80
#define MHL_DEV_LD_ALL				0xFF

enum {
	MHL_XDEVCAP_ECBUS_SPEEDS,
	MHL_XDEVCAP_TMDS_SPEEDS,
	MHL_XDEVCAP_ECBUS_ROLES,
	MHL_XDEVCAP_LOG_DEV_MAPX,
	MHL_XDEVCAP_SIZE
};

#define MHL_XDC_ECBUS_S_075			0x01
#define MHL_XDC_ECBUS_S_8BIT			0x02
#define MHL_XDC_ECBUS_S_12BIT			0x04
#define MHL_XDC_ECBUS_D_150			0x10
#define MHL_XDC_ECBUS_D_8BIT			0x20

#define MHL_XDC_TMDS_000			0x00
#define MHL_XDC_TMDS_150			0x01
#define MHL_XDC_TMDS_300			0x02
#define MHL_XDC_TMDS_600			0x04

#define MHL_XDC_DEV_HOST			0x01
#define MHL_XDC_DEV_DEVICE			0x02
#define MHL_XDC_DEV_CHARGER			0x04
#define MHL_XDC_HID_HOST			0x08
#define MHL_XDC_HID_DEVICE			0x10

#define MHL_XDC_LD_PHONE			0x01

enum {
	/* Command or Data byte acknowledge */
	MHL_ACK = 0x33,
	/* Command or Data byte not acknowledge */
	MHL_NACK = 0x34,
	/* Transaction abort */
	MHL_ABORT = 0x35,
	/* Write one status register strip top bit */
	MHL_WRITE_STAT = 0x60 | 0x80,
	/* Write one interrupt register */
	MHL_SET_INT = 0x60,
	/* Read one register */
	MHL_READ_DEVCAP_REG = 0x61,
	/* Read CBUS revision level from follower */
	MHL_GET_STATE = 0x62,
	/* Read vendor ID value from follower */
	MHL_GET_VENDOR_ID = 0x63,
	/* Set Hot Plug Detect in follower */
	MHL_SET_HPD = 0x64,
	/* Clear Hot Plug Detect in follower */
	MHL_CLR_HPD = 0x65,
	/* Set Capture ID for downstream device */
	MHL_SET_CAP_ID = 0x66,
	/* Get Capture ID from downstream device */
	MHL_GET_CAP_ID = 0x67,
	/* VS command to send RCP sub-commands */
	MHL_MSC_MSG = 0x68,
	/* Get Vendor-Specific command error code */
	MHL_GET_SC1_ERRORCODE = 0x69,
	/* Get DDC channel command error code */
	MHL_GET_DDC_ERRORCODE = 0x6A,
	/* Get MSC command error code */
	MHL_GET_MSC_ERRORCODE = 0x6B,
	/* Write 1-16 bytes to responder's scratchpad */
	MHL_WRITE_BURST = 0x6C,
	/* Get channel 3 command error code */
	MHL_GET_SC3_ERRORCODE = 0x6D,
	/* Write one extended status register */
	MHL_WRITE_XSTAT = 0x70,
	/* Read one extended devcap register */
	MHL_READ_XDEVCAP_REG = 0x71,
	/* let the rest of these float, they are software specific */
	MHL_READ_EDID_BLOCK,
	MHL_SEND_3D_REQ_OR_FEAT_REQ,
	MHL_READ_DEVCAP,
	MHL_READ_XDEVCAP
};

/* RAP action codes */
#define MHL_RAP_POLL		0x00	/* Just do an ack */
#define MHL_RAP_CONTENT_ON	0x10	/* Turn content stream ON */
#define MHL_RAP_CONTENT_OFF	0x11	/* Turn content stream OFF */
#define MHL_RAP_CBUS_MODE_DOWN	0x20
#define MHL_RAP_CBUS_MODE_UP	0x21

/* RAPK status codes */
#define MHL_RAPK_NO_ERR		0x00	/* RAP action recognized & supported */
#define MHL_RAPK_UNRECOGNIZED	0x01	/* Unknown RAP action code received */
#define MHL_RAPK_UNSUPPORTED	0x02	/* Rcvd RAP action code not supported */
#define MHL_RAPK_BUSY		0x03	/* Responder too busy to respond */

/*
 * Error status codes for RCPE messages
 */
/* No error. (Not allowed in RCPE messages) */
#define MHL_RCPE_STATUS_NO_ERROR		0x00
/* Unsupported/unrecognized key code */
#define MHL_RCPE_STATUS_INEFFECTIVE_KEY_CODE	0x01
/* Responder busy. Initiator may retry message */
#define MHL_RCPE_STATUS_BUSY			0x02

/*
 * Error status codes for RBPE messages
 */
/* No error. (Not allowed in RBPE messages) */
#define MHL_RBPE_STATUS_NO_ERROR		0x00
/* Unsupported/unrecognized button code */
#define MHL_RBPE_STATUS_INEFFECTIVE_BUTTON_CODE	0x01
/* Responder busy. Initiator may retry message */
#define MHL_RBPE_STATUS_BUSY			0x02

/*
 * Error status codes for UCPE messages
 */
/* No error. (Not allowed in UCPE messages) */
#define MHL_UCPE_STATUS_NO_ERROR		0x00
/* Unsupported/unrecognized key code */
#define MHL_UCPE_STATUS_INEFFECTIVE_KEY_CODE	0x01

#define MHL_STATUS_REG_CONNECTED_RDY		0x30
#define MHL_STATUS_REG_LINK_MODE		0x31
#define MHL_STATUS_REG_VERSION_STAT		0x32

#define MHL_STATUS_DCAP_RDY			0x01
#define MHL_STATUS_XDEVCAPP_SUPP		0x02
#define MHL_STATUS_POW_STAT			0x04
#define MHL_STATUS_PLIM_STAT_MASK		0x38

#define MHL_STATUS_CLK_MODE_MASK		0x07
#define MHL_STATUS_CLK_MODE_PACKED_PIXEL	0x02
#define MHL_STATUS_CLK_MODE_NORMAL		0x03
#define MHL_STATUS_PATH_EN_MASK			0x08
#define MHL_STATUS_PATH_ENABLED			0x08
#define MHL_STATUS_PATH_DISABLED		0x00
#define MHL_STATUS_MUTED_MASK			0x10

#define MHL_RCHANGE_INT				0x20
#define MHL_DCHANGE_INT				0x21

#define	MHL_INT_DCAP_CHG			0x01
#define MHL_INT_DSCR_CHG			0x02
#define MHL_INT_REQ_WRT				0x04
#define MHL_INT_GRT_WRT				0x08
#define MHL2_INT_3D_REQ				0x10
#define MHL3_INT_FEAT_REQ			0x20
#define MHL3_INT_FEAT_COMPLETE			0x40

/* On INTR_1 the EDID_CHG is located at BIT 0 */
#define MHL_INT_EDID_CHG			0x02

#endif /* LINUX_MHL_H */
