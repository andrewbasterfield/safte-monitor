#ifndef _QLOGIC_API_
#define _QLOGIC_API_

#include <linux/ioctl.h>

extern int qlogic_probe(int fd);
extern int qlogic_map_wwn_to_sd();
extern void qlogic_free();

#define	INT8	char
#define	INT16	short int
#define	INT32   int
#define	UINT8	unsigned char
#define	UINT16	unsigned short
#define	UINT32	unsigned int
#define	UINT64	void *
#define BOOLEAN unsigned char

#define	QL_DEF_WWN_NAME_SIZE		8
#define	QL_DEF_PORTID_SIZE		4

/* HBA node query interface type */
#define	QL_DEF_FC_INTF_TYPE		1
#define	QL_DEF_SCSI_INTF_TYPE		2

#define	QL_STATUS_DEV_NOT_FOUND		9

typedef struct {
	UINT16    Bus;				/* 2 */
	UINT16    Target;			/* 2 */
	UINT16    Lun;				/* 2 */
	UINT16    Padding[5];			/* 10 */
} QL_SCSI_ADDR;

typedef struct {
	UINT8     WWNN[QL_DEF_WWN_NAME_SIZE];	/* 8 */
	UINT8     WWPN[QL_DEF_WWN_NAME_SIZE];	/* 8 */
	UINT8     Id  [QL_DEF_PORTID_SIZE];
					/* 4; last 3 bytes used. big endian */

	/* The following fields currently are not supported */
	UINT16    Type;				/* 2; Port Type */
	UINT16    Status;			/* 2; Port Status */
	UINT16    Bus;				/* 2; n/a for Solaris */

	UINT16    TargetId;			/* 2 */
	UINT8     Local;			/* 1; Local or Remote */
	UINT8     ReservedByte[1];		/* 1 */
	
	UINT16    LoopID;			/* 2; Loop ID */
	
	UINT32    Reserved[7];			/* 28 */
} QL_DISC_PORT;

/* port type */
#define	QL_DEF_INITIATOR_DEV		1
#define	QL_DEF_TARGET_DEV		2
#define	QL_DEF_TAPE_DEV			4
#define	QL_DEF_FABRIC_DEV		8

typedef struct {
	UINT64    Signature;			/* 8 chars string */
	UINT16    AddrMode;			/* 2 */
	UINT16    Version;			/* 2 */
	UINT16    SubCode;			/* 2 */
	UINT16    Instance;			/* 2 */
	UINT32    Status;			/* 4 */
	UINT32    DetailStatus;			/* 4 */
	UINT32    Reserved1;			/* 4 */
	UINT32    RequestLen;			/* 4 */
	UINT32    ResponseLen;			/* 4 */
	UINT64    RequestAdr;			/* 8 */
	UINT64    ResponseAdr;			/* 8 */
	UINT16    HbaSelect;			/* 2 */
	UINT16    VendorSpecificStatus[11];	/* 22 */
	UINT64    VendorSpecificData;		/* 8 chars string */
} QL_IOCTL;

#define QL_MAX_FIBRE_DEVICES   256
#define	QL_VERSION	   5
#define QLMULTIPATH_MAGIC 'y'

#define QL_IOCTL_QUERY			         /* QUERY */	\
    _IOWR(QLMULTIPATH_MAGIC, 0x00, sizeof(QL_IOCTL))

#define	QL_IOCTL_SC_QUERY_HBA_NODE	1
#define	QL_IOCTL_SC_QUERY_HBA_PORT	2
#define	QL_IOCTL_SC_QUERY_DISC_PORT	3
#define	QL_IOCTL_SC_QUERY_DISC_TGT	4
#define	QL_IOCTL_SC_QUERY_DISC_LUN	5	/* Currently Not Supported */
#define	QL_IOCTL_SC_QUERY_DRIVER	6
#define	QL_IOCTL_SC_QUERY_FW		7
#define	QL_IOCTL_SC_QUERY_CHIP		8

#define QL_IOCTL_STARTIOCTL			 /* STARTIOCTL */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xff, sizeof(QL_IOCTL))
#define QL_IOCTL_SETINSTANCE			 /* SETINSTANCE */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xfe, sizeof(QL_IOCTL))
#define	QL_IOCTL_WWPN_TO_SCSIADDR		 /* WWPN_TO_SCSIADDR */ \
    _IOWR(QLMULTIPATH_MAGIC, 0xfd, sizeof(QL_IOCTL))

#endif
