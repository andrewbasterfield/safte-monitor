/*
 *  scsi_api.h - Interface to Linux generic SCSI
 *
 *  Author: Michael Clark <michael@metaparadigm.com>
 *  Copyright Metaparadigm Pte. Ltd. 2001
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef _SCSI_API_H_
#define _SCSI_API_H_

#include "scsi/scsi.h"
#include "scsi/sg.h"


#ifndef SCSI_IOCTL_GET_PCI
#define SCSI_IOCTL_GET_PCI 0x5387
#endif

#define MAX_ERRORS 5
#define MAX_SCSI_DEVS 255

#define SCSI_OFF sizeof(struct sg_header)

#define INQUIRY_CMD     0x12
#define INQUIRY_CMDLEN  6
#define INQUIRY_REPLY_LEN 96

#define READ_CMD     0x3c
#define READ_CMDLEN  10
#define READ_REPLY_LEN 256

#define INQUIRY_PERIPHERAL_OFFSET 0
#define INQUIRY_VENDOR_OFFSET  8
#define INQUIRY_VENDOR_LENGTH  8
#define INQUIRY_PRODUCT_OFFSET  16
#define INQUIRY_PRODUCT_LENGTH  16
#define INQUIRY_REVISION_OFFSET  32
#define INQUIRY_REVISION_LENGTH  4
#define INQUIRY_CHANNELID_OFFSET 43
#define INQUIRY_SAFTEID_OFFSET 44
#define INQUIRY_SAFTEID_LENGTH 6
#define INQUIRY_SERIAL_LENGTH 255

#define HOSTNAME_LEN 64
#define PREFIX_LEN 16
#define PCIINFO_LEN 32
#define PARAM_LEN 32
#define WWN_STR_LEN 16 /* 8 bytes as ascii hex */
#define PORTID_STR_LEN 6 /* 3 bytes as ascii hex */

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define msizeof(TYPE, MEMBER) ((size_t) sizeof(((TYPE *)0)->MEMBER))


typedef struct scsi_idlun {
/* why can't userland see this structure ??? */
    int dev_id;
    int host_unique_id;
} scsi_idlun_t;


typedef struct scsi_device {

  int active;
  char hostname[HOSTNAME_LEN+1];
  char pciinfo[PCIINFO_LEN+1];
  char *sg_device;
  char *device;
  char prefix[PREFIX_LEN+1];

  int host;
  int channel;
  int id;
  int lun;
  int part;
  int type;
  int channelid;
  int num_parts;

  /* scsi inquiry device info */
  char vendor[INQUIRY_VENDOR_LENGTH+1];
  char product[INQUIRY_PRODUCT_LENGTH+1];
  char revision[INQUIRY_REVISION_LENGTH+1];
  char serial[INQUIRY_SERIAL_LENGTH+1];
  char safteid[INQUIRY_SAFTEID_LENGTH+1];

  /* fibre channel addresses */
  int isfc;
  char wwpn[WWN_STR_LEN+1]; 
  char wwnn[WWN_STR_LEN+1]; 
  char portid[PORTID_STR_LEN+1];
  int loopid;


  struct scsi_device *next;

} scsi_device_t;


typedef struct fc_device {

  char wwpn[WWN_STR_LEN+1];
  char wwnn[WWN_STR_LEN+1];
  char portid[PORTID_STR_LEN+1];
  int loopid;

  int host;
  int channel;
  int id;
  int lun;

  struct fc_device *next;
} fc_device_t;

typedef int (*decode_func_t)(void *dest, const char *str, size_t size);
typedef int (*encode_func_t)(char *str, void *src, size_t size);

typedef struct scsi_device_param {

  char substchar;
  const char *name;
  const char *description;
  encode_func_t enc;
  decode_func_t dec;
  size_t offset;
  size_t size;
  
} scsi_device_param_t;


/* scsi device parameter definitions */
extern scsi_device_param_t params[];

/* global linked list of scsi devices */
extern scsi_device_t *scsidev_head;

/* Public functions */
extern int handle_scsi_cmd(int fd,
			   unsigned cmd_len,         /* command length */
			   unsigned in_size,         /* input data size */
			   unsigned char *i_buff,    /* input buffer */
			   unsigned out_size,        /* output data size */
			   unsigned char *o_buff     /* output buffer */);
extern unsigned char *scsi_inquiry (int fd, int evpd, int pg);
extern int get_scsi_dev_info(int fd, scsi_device_t *scsidev);
extern void make_dev_name(char * fname, const char * leadin, int k, 
                          int do_numeric);
extern scsi_device_t* find_dev_by_loc(int host, int channel, int id, int lun);
extern scsi_device_t* find_dev_by_name(const char* device);
extern void map_sg_devices(int type, const char* prefix, int numeric);
extern int scan_scsi_devices(int sg_numeric);
extern void print_scsi_dev_info(scsi_device_t *scsidev);
extern int free_scsidev(scsi_device_t *scsidev);

#endif
