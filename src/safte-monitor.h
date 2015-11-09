/*
 *  safte-monitor - SCSI enclosure monitor
 *
 *  Author: Michael Clark <michael@metaparadigm.com>
 *  Copyright Metaparadigm Pte. Ltd. 2001
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#ifndef _SAFTEMON_H_
#define _SAFTEMON_H_

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif

#include "scsi_api.h"


#define SAFTE_MAX_SLOTS 256
#define SAFTE_MAX_FAN 16
#define SAFTE_MAX_PSU 16
#define SAFTE_MAX_TEMPSENSORS 16

/* SAF-TE Read operations */
#define SAFTE_READ_ENCLOSURE_CONFIG 0x00
#define SAFTE_READ_ENCLOSURE_STATUS 0x01
#define SAFTE_READ_USAGE_STATISTICS 0x02
#define SAFTE_READ_DEVICE_INSERTIONS 0x03
#define SAFTE_READ_DEVICE_SLOT_STATUS 0x04
#define SAFTE_READ_GLOBAL_FLAGS 0x05

/* SAF-TE Write operations */
#define SAFTE_WRITE_DEVICE_SLOT_STATUS 0x10
#define SAFTE_SET_SCSI_ID 0x11
#define SAFTE_PERFORM_SLOT_OPERATION 0x12
#define SAFTE_SET_FAN_SPEED 0x13
#define SAFTE_ACTIVATE_POWER_SUPPLY 0x14
#define SAFTE_SEND_GLOBAL_FLAGS 0x15

/* Status codes for READ_DEVICE_SLOT_STATUS */
#define SAFTE_SLOT_BYTE0_NOERROR 0x01
#define SAFTE_SLOT_BYTE0_FAULTY 0x02
#define SAFTE_SLOT_BYTE0_REBUILDING 0x04
#define SAFTE_SLOT_BYTE0_FAILEDARRAY 0x08
#define SAFTE_SLOT_BYTE0_CRITICALARRAY 0x10
#define SAFTE_SLOT_BYTE0_PARITYCHECK 0x20
#define SAFTE_SLOT_BYTE0_PREDICTFAULT 0x40
#define SAFTE_SLOT_BYTE0_UNCONFIGURED 0x80
#define SAFTE_SLOT_BYTE3_NOTPRESENT 0x00
#define SAFTE_SLOT_BYTE3_PRESENT 0x01
#define SAFTE_SLOT_BYTE3_INSERTREADY 0x02
#define SAFTE_SLOT_BYTE3_ACTIVE 0x04

/* Status codes for READ_ENCLOSURE_STATUS */
#define SAFTE_FAN_STATUS_OPERATIONAL 0x00
#define SAFTE_FAN_STATUS_MALFUNCTION 0x01
#define SAFTE_FAN_STATUS_NOTINSTALLED 0x02
#define SAFTE_FAN_STATUS_UNKNOWN 0x80
#define SAFTE_PSU_STATUS_OKAY_ON 0x00
#define SAFTE_PSU_STATUS_OKAY_OFF 0x01
#define SAFTE_PSU_STATUS_MALFUNCTION_ON 0x10
#define SAFTE_PSU_STATUS_MALFUNCTION_OFF 0x11
#define SAFTE_PSU_STATUS_NOTPRESENT 0x20
#define SAFTE_PSU_STATUS_PRESENT 0x21
#define SAFTE_PSU_STATUS_UNKNOWN 0x80
#define SAFTE_DOOR_STATUS_LOCKED 0x00
#define SAFTE_DOOR_STATUS_UNLOCKED 0x01
#define SAFTE_DOOR_STATUS_UNKNOWN 0x80
#define SAFTE_SPEAKER_STATUS_OFF 0x00
#define SAFTE_SPEAKER_STATUS_ON 0x01

/* internally used status codes */
#define SAFTE_TEMP_STATUS_OKAY 0x00
#define SAFTE_TEMP_STATUS_ALERT 0x01

/* number of SAF-TE devices found */
extern int safte_num;

typedef struct safte_slot {

  int id;
  int status0;
  int status1;
  int status2;
  int status3;
  int insertions;

  scsi_device_t *device;

} safte_slot_t;


typedef struct safte_device {

  scsi_device_t *device;

  int fans;
  int psus;
  int slots;
  int doorlocks;
  int tempsensors;
  int audiblealarm;
  int thermostats;

  safte_slot_t slot[SAFTE_MAX_SLOTS];
  int fan[SAFTE_MAX_FAN];
  int psu[SAFTE_MAX_PSU];
  int doorlock;
  int speaker;
  float temp[SAFTE_MAX_TEMPSENSORS];
  int temp_oor[SAFTE_MAX_TEMPSENSORS]; /* out of range */
  int temp_alert;
  int celsius_flag;

  struct safte_device *copy;

  struct safte_device *next;

} safte_device_t;


#define SAFTE_SLOT_BYTE0_STATUS 1
#define SAFTE_SLOT_BYTE3_STATUS 2
#define SAFTE_FAN_STATUS 3
#define SAFTE_PSU_STATUS 4
#define SAFTE_DOOR_STATUS 5
#define SAFTE_SPEAKER_STATUS 6
#define SAFTE_TEMP_STATUS 7

typedef struct safte_status_code {
  int system;
  int code;
  int severity;
  char *desc;
} safte_status_code_t;

#endif


