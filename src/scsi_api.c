/*
 *  scsi_api.c - Interface to Linux generic SCSI
 *
 *  Author: Michael Clark <michael@metaparadigm.com>
 *  Copyright Metaparadigm Pte. Ltd. 2001
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <signal.h>
#include <syslog.h>

#include "scsi_api.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


/* global linked list of scsi devices */
scsi_device_t *scsidev_head = NULL;


static int decode_int(void *dest, const char *str, size_t size)
{
    return sscanf(str, "%d", (int*)dest);
}


static int encode_int(char *str, void *src, size_t size)
{
    return snprintf(str, size, "%d", *(int*)src);
}


static int decode_str(void *dest, const char *str, size_t size)
{
    int l = strlen(strncpy((char*)dest, str, size));
    return (l == size) ? -1 : l;
}


static int encode_str(char *str, void *src, size_t size)
{
    int l = strlen(strncpy(str, (char*)src, size));
    return (l == size) ? -1 : l;
}


static int encode_strp(char *str, void *src, size_t size)
{
    if(*((char**)src)) {
	int l = strlen(strncpy(str, *((char**)src), size));
	return (l == size) ? -1 : l;
    } else {
	str[0] = '\0';
	return 0;
    }
}


static int decode_strp(void *dest, const char *str, size_t size)
{
    int l = strlen(*((char**)dest) = strdup(str));
    return (l == size) ? -1 : l;
}


static int decode_type(void *dest, const char *str, size_t size)
{
    if(strcmp(str, "disk") == 0) {
	*(int*)dest = TYPE_DISK;
	return 1;
    } else if(strcmp(str, "processor") == 0) {
	*(int*)dest = TYPE_PROCESSOR;
	return 1;
    } else if(strcmp(str, "cdrom") == 0) {
	*(int*)dest = TYPE_ROM;
	return 1;
    } else if(strcmp(str, "worm") == 0) {
	*(int*)dest = TYPE_WORM;
	return 1;
    } else if(strcmp(str, "tape") == 0) {
	*(int*)dest = TYPE_TAPE;
	return 1;
    } else if(strcmp(str, "mod") == 0) {
	*(int*)dest = TYPE_MOD;
	return 1;
    } else if(strcmp(str, "scanner") == 0) {
	*(int*)dest = TYPE_SCANNER;
	return 1;
    }
    return 0;
}


static int encode_type(char *str, void *src, size_t size)
{
    switch(*(int*)src) {
    case TYPE_DISK:
	return snprintf(str, size, "%s", "disk");
	break;
    case TYPE_PROCESSOR:
	return snprintf(str, size, "%s", "processor");
	break;
    case TYPE_ROM:
	return snprintf(str, size, "%s", "cdrom");
	break;
    case TYPE_WORM:
	return snprintf(str, size, "%s", "worm");
	break;
    case TYPE_TAPE:
	return snprintf(str, size, "%s", "tape");
	break;
    case TYPE_MOD:
	return snprintf(str, size, "%s", "mod");
	break;
    case TYPE_SCANNER:
	return snprintf(str, size, "%s", "scanner");
	break;
    default:
	return snprintf(str, size, "%s", "other");
    }
}


scsi_device_param_t params[] = {
    { 'h', "host", "Host / adapter no", &encode_int, &decode_int,
      offsetof(scsi_device_t, host), 0 },
    { 'c', "channel", "Channel no", &encode_int, &decode_int,
      offsetof(scsi_device_t, channel), 0 },
    { 't', "id", "Target / scsi id", &encode_int, &decode_int,
      offsetof(scsi_device_t, id), 0 },
    { 'l', "lun", "Lun", &encode_int, &decode_int,
      offsetof(scsi_device_t, lun), 0 },
    { 'p', "part", "Partition number", &encode_int, &decode_int,
      offsetof(scsi_device_t, part), 0 },
    { 'w', "fcwwnn", "FC World Wide Node Name", &encode_str, &decode_str,
      offsetof(scsi_device_t, wwnn), msizeof(scsi_device_t, wwnn) },
    { 'W', "fcwwpn", "FC World Wide Port Name", &encode_str, &decode_str,
      offsetof(scsi_device_t, wwpn), msizeof(scsi_device_t, wwpn) },
    { 'I', "fcdid", "FC Port ID", &encode_str, &decode_str,
      offsetof(scsi_device_t, portid), msizeof(scsi_device_t, portid) },
    { 'L', "fclid", "FC Loop ID", &encode_int, &decode_int,
      offsetof(scsi_device_t, loopid), 0 },
    { 'B', "pci", "PCI bus:dev.func", &encode_str, &decode_str,
      offsetof(scsi_device_t, pciinfo), msizeof(scsi_device_t, pciinfo) },
    { 'H', "hostname", "HBA Driver name", &encode_str, &decode_str,
      offsetof(scsi_device_t, hostname), msizeof(scsi_device_t, hostname) },
    { 'V', "vendor", "Vendor Inquiry info", &encode_str, &decode_str,
      offsetof(scsi_device_t, vendor), msizeof(scsi_device_t, vendor) },
    { 'P', "product", "Product Inquiry info", &encode_str, &decode_str,
      offsetof(scsi_device_t, product), msizeof(scsi_device_t, product) },
    { 'R', "revision", "Revision Inquiry info", &encode_str, &decode_str,
      offsetof(scsi_device_t, revision), msizeof(scsi_device_t, revision) },
    { 'S', "serial", "Serial number Inquiry info", &encode_str, &decode_str,
      offsetof(scsi_device_t, serial), msizeof(scsi_device_t, serial) },
    { 'D', "prefix", "Device prefix (sd, scd)", &encode_str, &decode_str,
      offsetof(scsi_device_t, prefix), msizeof(scsi_device_t, prefix) },
    { 'T', "type", "Device type (disk, cdrom)", &encode_type, &decode_type,
      offsetof(scsi_device_t, type), 0 },
    { 'g', "device", "Generic device (/dev/sg0)", &encode_strp, &decode_strp,
      offsetof(scsi_device_t, device), msizeof(scsi_device_t, device) },
    { 'd', "sg_device", "Device (/dev/sda)", &encode_strp, &decode_strp,
      offsetof(scsi_device_t, sg_device), msizeof(scsi_device_t, sg_device) },
    { '\0', "", "", NULL, NULL, 0, 0 }
};


/* process a complete SCSI cmd. Use the generic SCSI interface. */
int handle_scsi_cmd(int fd,
		    unsigned cmd_len,         /* command length */
		    unsigned in_size,         /* input data size */
		    unsigned char *i_buff,    /* input buffer */
		    unsigned out_size,        /* output data size */
		    unsigned char *o_buff     /* output buffer */)
{
    int status = 0;
    struct sg_header *sg_hd;

    /* safety checks */
    if (!cmd_len) return -1;            /* need a cmd_len != 0 */
    if (!i_buff) return -1;             /* need an input buffer != NULL */
    if (SCSI_OFF + cmd_len + in_size > 4096) return -1;
    if (SCSI_OFF + out_size > 4096) return -1;

    if (!o_buff) out_size = 0;      /* no output buffer, no output size */

    /* generic SCSI device header construction */
    sg_hd = (struct sg_header *) i_buff;
    sg_hd->reply_len   = SCSI_OFF + out_size;
    sg_hd->twelve_byte = cmd_len == 12;
    sg_hd->result = 0;

    /* send command */
    status = write( fd, i_buff, SCSI_OFF + cmd_len + in_size );
    if ( status < 0 || status != SCSI_OFF + cmd_len + in_size || 
	 sg_hd->result ) {
	/* some error happened */
	fprintf( stderr, "write(generic) result = 0x%x cmd = 0x%x\n",
		 sg_hd->result, i_buff[SCSI_OFF] );
	perror("");
	return status;
    }
    
    if (!o_buff) o_buff = i_buff;       /* buffer pointer check */

    /* retrieve result */
    status = read( fd, o_buff, SCSI_OFF + out_size);
    if ( status < 0 || status != SCSI_OFF + out_size || sg_hd->result ) {
	/* some error happened */
	fprintf( stderr, "read(generic) status = 0x%x, result = 0x%x, "
		 "cmd = 0x%x\n", 
		 status, sg_hd->result, o_buff[SCSI_OFF] );
	fprintf( stderr, "read(generic) sense "
		 "%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n", 
		 sg_hd->sense_buffer[0], sg_hd->sense_buffer[1],
		 sg_hd->sense_buffer[2], sg_hd->sense_buffer[3],
		 sg_hd->sense_buffer[4], sg_hd->sense_buffer[5],
		 sg_hd->sense_buffer[6], sg_hd->sense_buffer[7],
		 sg_hd->sense_buffer[8], sg_hd->sense_buffer[9],
		 sg_hd->sense_buffer[10], sg_hd->sense_buffer[11],
		 sg_hd->sense_buffer[12], sg_hd->sense_buffer[13],
		 sg_hd->sense_buffer[14], sg_hd->sense_buffer[15]);
	if (status < 0)
	    perror("");
    }
    /* Look if we got what we expected to get */
    if (status == SCSI_OFF + out_size) status = 0; /* got them all */

    return status;  /* 0 means no error */
}


/* request vendor brand and model - use evpd=0, op=0 */
/* request serial number - use evpd=1, op=0x80 */
unsigned char *scsi_inquiry (int fd, int evpd, int pg)
{
    static unsigned char cmd[SCSI_OFF + 18];      /* SCSI command buffer */
    static unsigned char scsi_inquiry_buffer[SCSI_OFF + INQUIRY_REPLY_LEN];
    unsigned char cmdblk [ INQUIRY_CMDLEN ] = 
    { INQUIRY_CMD,  /* command */
      evpd,  /* lun/reserved */
      pg,  /* page code */
      0,  /* reserved */
      INQUIRY_REPLY_LEN,  /* allocation length */
      0 };/* reserved/flag/link */

    memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

    if (handle_scsi_cmd(fd, sizeof(cmdblk), 0, cmd,
			sizeof(scsi_inquiry_buffer) - SCSI_OFF,
			scsi_inquiry_buffer)) {
	fprintf( stderr, "Inquiry failed\n" );
	exit(2);
    }
    return (scsi_inquiry_buffer + SCSI_OFF);
}


static void str_trim(char *p)
{
    char *t = p + strlen(p) - 1;
    while(t >= p) {
	if(*t == 0x20) *t-- = '\0';
	else break;
    }
}


int get_scsi_dev_info(int fd, scsi_device_t *scsidev)
{
    unsigned char *buf;
    Sg_scsi_id scsi_id;
    char hostname[HOSTNAME_LEN+1];
    int l;

    /* Get host no, channel and scsi id */
    if(ioctl(fd, SG_GET_SCSI_ID, &scsi_id) < 0) {
	perror("ioctl");
	exit(1);
    }

    scsidev->host = scsi_id.host_no;
    scsidev->channel = scsi_id.channel;
    scsidev->id = scsi_id.scsi_id;
    scsidev->lun = scsi_id.lun;

    /* Get scsi inquiry info */
    buf = scsi_inquiry(fd, 0, 0);

    scsidev->active = 1;

    scsidev->type = *(buf+INQUIRY_PERIPHERAL_OFFSET);

    memcpy(scsidev->vendor,
	   buf + INQUIRY_VENDOR_OFFSET, INQUIRY_VENDOR_LENGTH);
    scsidev->vendor[INQUIRY_VENDOR_LENGTH] = '\0';
    str_trim(scsidev->vendor);

    memcpy(scsidev->revision,
	   buf + INQUIRY_REVISION_OFFSET, INQUIRY_REVISION_LENGTH);
    scsidev->revision[INQUIRY_REVISION_LENGTH] = '\0';
    str_trim(scsidev->revision);

    memcpy(scsidev->product,
	   buf + INQUIRY_PRODUCT_OFFSET, INQUIRY_PRODUCT_LENGTH);
    scsidev->product[INQUIRY_PRODUCT_LENGTH] = '\0';
    str_trim(scsidev->product);

    memcpy(scsidev->safteid, buf + INQUIRY_SAFTEID_OFFSET, INQUIRY_SAFTEID_LENGTH);
    scsidev->safteid[INQUIRY_SAFTEID_LENGTH] = '\0';

    scsidev->channelid = *(buf+INQUIRY_CHANNELID_OFFSET);

    /* Get serial number */
    buf = scsi_inquiry(fd, 1, 0x80);
    l = buf[3];
    memcpy(scsidev->serial, buf + 4, l);
    scsidev->serial[l] = '\0';
    str_trim(scsidev->serial);

    /* Get the hostname */
    *(int*)hostname = HOSTNAME_LEN;
    if(ioctl (fd, SCSI_IOCTL_PROBE_HOST, hostname) >= 0) {
	strncpy(scsidev->hostname, hostname, HOSTNAME_LEN);
	str_trim(scsidev->hostname);
    }

    /* Get the PCI bus:dev.fn */
    if(ioctl(fd, SCSI_IOCTL_GET_PCI, scsidev->pciinfo) < 0) {
	/* we ignore the error, either ioctl is not supported
	   or the scsi device is not PCI (ide-scsi) */
    }

    return 0;
}


void make_dev_name(char * fname, const char * leadin, int k, 
		   int do_numeric)
{
    char buff[64];
    int  big,little;

    if(leadin[0] == '/') strcpy(fname, leadin);
    else sprintf(fname, "/dev/%s", leadin);
    if (do_numeric) {
	sprintf(buff, "%d", k);
	strcat(fname, buff);
    }
    else {
	if (k < 26) {
	    buff[0] = 'a' + (char)k;
	    buff[1] = '\0';
	    strcat(fname, buff);
	}
	else if (k <= 255) {
	    /* assumes sequence goes x,y,z,aa,ab,ac etc */
	    big    = k/26;
	    little = k - (26 * big);
	    big    = big - 1;

	    buff[0] = 'a' + (char)big;
	    buff[1] = 'a' + (char)little;
	    buff[2] = '\0';
	    strcat(fname, buff);
	}
	else
	    strcat(fname, "xxxx");
    }
}


scsi_device_t* find_dev_by_loc(int host, int channel, int id, int lun)
{
    scsi_device_t *scsidev = scsidev_head;

    while(scsidev->next) {
	if (host == scsidev->host && channel == scsidev->channel &&
	    id == scsidev->id && lun == scsidev->lun) return scsidev;
	scsidev = scsidev->next;
    }
    return NULL;
}


scsi_device_t* find_dev_by_name(const char* device)
{
    scsi_device_t *scsidev = scsidev_head;
    char device_temp[PATH_MAX+1];


    if(device[0] == '/') strcpy(device_temp, device);
    else sprintf(device_temp, "/dev/%s", device);

    while(scsidev->next) {
	if (scsidev->device && strcmp(device_temp, scsidev->device) == 0)
	    return scsidev;
	scsidev = scsidev->next;
    }
    return NULL;
}


void map_sg_devices(int type, const char* prefix, int numeric)
{
    scsi_device_t *scsidev = NULL;
    int fd, k, res, num_errors = 0;
    int host, channel, id, lun;
    scsi_idlun_t idlun;
    char device[PATH_MAX+1];

    /* find sd? devices that map to sg? devices */
    for (k = 0, res = 0; num_errors < MAX_ERRORS; k++) {

	make_dev_name(device, prefix, k, numeric);

	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
	    if (EBUSY == errno) {
		printf("Device %s is busy\n", device);
		++num_errors;
		continue;
	    }
	    else if ((ENODEV == errno) || (ENOENT == errno) ||
		     (ENXIO == errno)) {
		++num_errors;
		continue;
	    }
	    else {
		perror("open");
		++num_errors;
		continue;
	    }
	}

	res = ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun);
	if (res < 0) {
	    perror("ioctl");
	    ++num_errors;
	    continue;
	}
	res = ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &host);
	if (res < 0) {
	    perror("ioctl");
	    ++num_errors;
	    continue;
	}

	id = idlun.dev_id & 0xff;
	lun = (idlun.dev_id >> 8) & 0xff;
	channel = (idlun.dev_id >> 16) & 0xff;

	scsidev = find_dev_by_loc(host, channel, id, lun);
	if (scsidev && scsidev->type == type) {
	    scsidev->device = strdup(device);
	    strncpy(scsidev->prefix, prefix, PREFIX_LEN);
	    /* printf("%s - %d %d %d %d maps to %s\n",
	       device, host, channel, id, lun, scsidev->sg_device); */
	} else {
	    /* printf("can't find sd device for %s", scsidev->sg_device); */
	}

	close(fd);

    }
}


int scan_scsi_devices(int sg_numeric)
{
    char tmp[PATH_MAX];
    scsi_device_t *scsidev = scsidev_head;
    int fd, k, num_errors = 0;

    for (k = 0; (k < MAX_SCSI_DEVS)  && (num_errors < MAX_ERRORS); k++) {

	make_dev_name(tmp, "/dev/sg", k, sg_numeric);
	scsidev->sg_device = strdup(tmp);

	fd = open(scsidev->sg_device, O_RDWR);

	if (fd < 0) {
	    if (EBUSY == errno) {
		scsidev->active = -2;
		continue;
	    } else if ((ENODEV == errno) ||
		       (ENOENT == errno) || (ENXIO == errno)) {
		++num_errors;
		scsidev->active = -1;
		continue;
	    } else {
		scsidev->active = 0;
		++num_errors;
		continue;
	    }
	}

	get_scsi_dev_info(fd, scsidev);
#if DEBUG
	print_scsi_dev_info(scsidev);
#endif
	close(fd);
	scsidev->next = calloc(1, sizeof(scsi_device_t));
	scsidev = scsidev->next;
    }

    return 0;
}


void print_scsi_dev_info(scsi_device_t *scsidev)
{
    char scsiinfo[1024] = "";
    char tmpbuf[128] = "";
    char* scsicodes = "ThctlVPRSBdg";
    char* fccodes = "wWIL";
    char *code;

    code = scsicodes;
    while(*code) {
	scsi_device_param_t *param = params;
	while(param->substchar) {
	    if(param->substchar == *code) break;
	    param++;
	}
	if(param->enc) {
	    if(param->enc(tmpbuf, ((void*)scsidev)+param->offset, 128) > 0) {
		if(scsiinfo[0]) strncat(scsiinfo, " ", 1024-strlen(scsiinfo));
		strncat(scsiinfo, param->name, 1024-strlen(scsiinfo));
		strncat(scsiinfo, "=", 1024-strlen(scsiinfo));
		if(param->enc != encode_int)
		    strncat(scsiinfo, "\"", 1024-strlen(scsiinfo));
		strncat(scsiinfo, tmpbuf, 1024-strlen(scsiinfo));
		if(param->enc != encode_int)
		    strncat(scsiinfo, "\"", 1024-strlen(scsiinfo));
	    }
	}
	code++;
    }
    if(!scsidev->isfc) goto notfc;
    code = fccodes;
    while(*code) {
	scsi_device_param_t *param = params;
	while(param->substchar) {
	    if(param->substchar == *code) break;
	    param++;
	}
	if(param->enc) {
	    if(param->enc(tmpbuf, ((void*)scsidev)+param->offset, 128) > 0) {
		strncat(scsiinfo, " ", 1024-strlen(scsiinfo));
		strncat(scsiinfo, param->name, 1024-strlen(scsiinfo));
		strncat(scsiinfo, "=", 1024-strlen(scsiinfo));
		if(param->enc != encode_int)
		    strncat(scsiinfo, "\"", 1024-strlen(scsiinfo));
		strncat(scsiinfo, tmpbuf, 1024-strlen(scsiinfo));
		if(param->enc != encode_int)
		    strncat(scsiinfo, "\"", 1024-strlen(scsiinfo));
	    }
	}
	code++;
    }

 notfc:
    printf("%s\n", scsiinfo);
}


int free_scsidev(scsi_device_t *scsidev)
{
    scsi_device_t *c_scsidev = scsidev;

    while(c_scsidev->next) {
	scsi_device_t *t = c_scsidev;
	c_scsidev = c_scsidev->next;
	if(t->device) free(t->device);
	if(t->sg_device) free(t->sg_device);
	free(t);
    }
    return 0;
}


