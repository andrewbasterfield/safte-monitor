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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "safte-monitor.h"
#include "mathopd.h"

/* max temperature for alert */
#ifdef USE_CELCIUS
#define MAX_TEMP_DEFAULT 35.0
#define TEMP_UNIT "c"
#else
#define MAX_TEMP_DEFAULT 95.0
#define TEMP_UNIT "f"
#endif

/* command line flags */
static int print_flag = 0;      /* print device scan information */
static int sg_numeric = 1;      /* use numeric sg device names */
static int log_temp = 0;        /* log temperature changes */
static int alert_noncrit = 0;   /* alert for non critical state changes */
static char* alert_prog = NULL; /* alert notifcation program */
static float max_temp = MAX_TEMP_DEFAULT; /* max temp */

safte_device_t *saftedev_head = NULL;

int safte_num;

/* Status codes decoding table */
safte_status_code_t statuscodes[] = {
  {SAFTE_SLOT_BYTE3_STATUS, SAFTE_SLOT_BYTE3_NOTPRESENT, 0,
   "disk not present"},
  {SAFTE_SLOT_BYTE3_STATUS, SAFTE_SLOT_BYTE3_PRESENT, 0,
   "disk present"},
  {SAFTE_SLOT_BYTE3_STATUS, SAFTE_SLOT_BYTE3_INSERTREADY, 0,
   "insert ready"},
  {SAFTE_SLOT_BYTE3_STATUS, SAFTE_SLOT_BYTE3_ACTIVE, 0,
   "active"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_NOERROR, 0,
   "no error"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_FAULTY, 1,
   "faulty"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_REBUILDING, 1,
   "rebuilding"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_FAILEDARRAY, 1,
   "failed array"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_CRITICALARRAY, 1,
   "critical array"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_PARITYCHECK, 1,
   "parity check"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_PREDICTFAULT, 1,
   "predicted fault"},
  {SAFTE_SLOT_BYTE0_STATUS, SAFTE_SLOT_BYTE0_UNCONFIGURED, 0,
   "unconfigured"},
  {SAFTE_FAN_STATUS, SAFTE_FAN_STATUS_OPERATIONAL, 0,
   "operational"},
  {SAFTE_FAN_STATUS, SAFTE_FAN_STATUS_MALFUNCTION, 1,
   "malfunctioning"},
  {SAFTE_FAN_STATUS, SAFTE_FAN_STATUS_NOTINSTALLED, 0,
   "not installed"},
  {SAFTE_FAN_STATUS, SAFTE_FAN_STATUS_UNKNOWN, 1,
   "unknown"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_OKAY_ON, 0,
   "okay and on"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_OKAY_OFF, 0,
   "okay and off"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_MALFUNCTION_ON, 1,
   "malfunctioning and commanded on"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_MALFUNCTION_OFF, 1,
   "malfunctioning and commaned off"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_NOTPRESENT, 1,
   "notpresent"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_PRESENT, 0,
   "present"},
  {SAFTE_PSU_STATUS, SAFTE_PSU_STATUS_UNKNOWN, 1,
   "unknown"},
  {SAFTE_DOOR_STATUS, SAFTE_DOOR_STATUS_LOCKED, 0,
   "locked"},
  {SAFTE_DOOR_STATUS, SAFTE_DOOR_STATUS_UNLOCKED, 1,
   "unlocked"},
  {SAFTE_DOOR_STATUS, SAFTE_DOOR_STATUS_UNKNOWN, 0,
   "unknown"},
  {SAFTE_SPEAKER_STATUS, SAFTE_SPEAKER_STATUS_OFF, 0,
   "off"},
  {SAFTE_SPEAKER_STATUS, SAFTE_SPEAKER_STATUS_ON, 1,
   "on"},
  {SAFTE_TEMP_STATUS, SAFTE_TEMP_STATUS_OKAY, 0,
   "okay"},
  {SAFTE_TEMP_STATUS, SAFTE_TEMP_STATUS_ALERT, 1,
   "alert"},
  {0, 0, 0, NULL}
};


/* request saf-te data */
static unsigned char *safte_read (int fd, int safte_cmd)
{
  static unsigned char cmd[SCSI_OFF + 18];      /* SCSI command buffer */
  static unsigned char safte_read_buffer[ SCSI_OFF + READ_REPLY_LEN ];
  unsigned char cmdblk [ READ_CMDLEN ] = 
  { READ_CMD,  /* command */
    1,  /* lun/reserved/mode */
    safte_cmd,  /* buffer id */
    0,  /* reserved */
    0,  /* reserved */
    0,  /* reserved */
    0,  /* reserved */
    READ_REPLY_LEN / 0x100,  /* allocation length MSB */
    READ_REPLY_LEN % 0x100,  /* allocation length LSB */
    0 };/* reserved/flag/link */

  memcpy( cmd + SCSI_OFF, cmdblk, sizeof(cmdblk) );

  if (handle_scsi_cmd(fd, sizeof(cmdblk), 0, cmd, 
                      sizeof(safte_read_buffer) - SCSI_OFF,
		      safte_read_buffer )) {
    fprintf( stderr, "read failed\n" );
    exit(2);
  }
  return (safte_read_buffer + SCSI_OFF);
}


int get_safte_enclosure_config(int fd, safte_device_t *safte_dev)
{
  unsigned char* buf;

  buf = safte_read(fd, SAFTE_READ_ENCLOSURE_CONFIG);

  safte_dev->fans = *(buf);
  safte_dev->psus = *(buf+1);
  safte_dev->slots = *(buf+2);
  safte_dev->doorlocks = *(buf+3);
  safte_dev->tempsensors = *(buf+4);
  safte_dev->audiblealarm = *(buf+5); 
  safte_dev->thermostats = *(buf+6) & 0x7f;
  safte_dev->celsius_flag = *(buf+6) & 0x80;

  return 0;
}

int get_safte_enclosure_status(int fd, safte_device_t *safte_dev)
{
  unsigned char* buf;
  int i;
  int toorf;

  buf = safte_read(fd, SAFTE_READ_ENCLOSURE_STATUS);

  for(i=0; i < safte_dev->fans; i++) {
    safte_dev->fan[i] = *(buf + i);
  }

  for(i=0; i < safte_dev->psus; i++) {
    safte_dev->psu[i] = *(buf + safte_dev->fans + i);
  }

  for(i=0; i < safte_dev->slots; i++) {
    safte_dev->slot[i].id = *(buf + safte_dev->fans + safte_dev->psus + i);
  }

  safte_dev->doorlock = *(buf + safte_dev->fans + safte_dev->psus +
			  safte_dev->slots);

  safte_dev->speaker = *(buf + safte_dev->fans + safte_dev->psus +
			 safte_dev->slots + i);

  for(i=0; i < safte_dev->tempsensors; i++) {
	safte_dev->temp[i] = *(buf + safte_dev->fans + safte_dev->psus +
		   safte_dev->slots + 2 + i) - 10;
#ifdef USE_CELCIUS
	if ( ! safte_dev->celsius_flag )
		/* Convert from Fahrenheit. */
		safte_dev->temp[i] = (safte_dev->temp[i] - 32.0) * 5.0 / 9.0;
#else
	if ( safte_dev->celsius_flag )
		safte_dev->temp[i] = (safte_dev->temp[i] * 9.0 / 5.0) + 32.0;
#endif
  }
  toorf = *(buf + safte_dev->fans + safte_dev->psus + safte_dev->slots +
	    safte_dev->tempsensors + 2 + i);
  toorf <<= 8;
  toorf &= *(buf + safte_dev->fans + safte_dev->psus + safte_dev->slots +
	     safte_dev->tempsensors + 3 + i);
  for(i=0; i < safte_dev->tempsensors; i++) {
    safte_dev->temp_oor[i] = (toorf & (1 << i)) ? 1 : 0;
  }
  safte_dev->temp_alert = (toorf & 0xf000) ? 1 : 0;

  return 0;
}

int get_safte_device_insertions(int fd, safte_device_t *safte_dev)
{
  unsigned char* buf;
  int i;

  buf = safte_read(fd, SAFTE_READ_DEVICE_INSERTIONS);

  for(i=0; i < safte_dev->slots; i++) {
    safte_dev->slot[i].insertions = (*(buf + i*2) << 8) + *(buf + i*2 + 1);
  }

  return 0;
}

int get_safte_device_slot_status(int fd, safte_device_t *safte_dev)
{
  unsigned char* buf;
  int i;

  buf = safte_read(fd, SAFTE_READ_DEVICE_SLOT_STATUS);

  for(i=0; i < safte_dev->slots; i++) {
    safte_dev->slot[i].status0 = *(buf + i*4);
    safte_dev->slot[i].status1 = *(buf + i*4 + 1);
    safte_dev->slot[i].status2 = *(buf + i*4 + 2);
    safte_dev->slot[i].status3 = *(buf + i*4 + 3);
  }

  return 0;
}


int map_slots_to_devices()
{
  int i;
  safte_device_t *saftedev;
  scsi_device_t *scsidev;

  scsidev = scsidev_head;
  while(scsidev->next) {
    saftedev = saftedev_head;
    while(saftedev->next) {
      for(i=0; i < saftedev->slots; i++)
	{
	  if(saftedev->device->host == scsidev->host &&
	     saftedev->device->channel == scsidev->channel &&
	     saftedev->slot[i].id == scsidev->id && scsidev->device)
	    {
	      saftedev->slot[i].device = scsidev;
	    }
	}
      saftedev = saftedev->next;
    }
    scsidev = scsidev->next;
  }

  return 0;
}


int scan_safte_devices()
{
  int fd, safte_num = 0;
  safte_device_t *saftedev = saftedev_head;
  scsi_device_t *scsidev = scsidev_head;

  while(scsidev->next) {
    if(scsidev->type == TYPE_PROCESSOR &&
       strncmp(scsidev->safteid, "SAF-TE", 6) == 0) {
	
      saftedev->device = scsidev;
      fd = open(scsidev->sg_device, O_RDWR);
      if (fd < 0) {
	perror("open");
	exit(1);
      }
      get_safte_enclosure_config(fd, saftedev);
      get_safte_enclosure_status(fd, saftedev);
      get_safte_device_insertions(fd, saftedev);
      close(fd);
      saftedev->next = calloc(1, sizeof(safte_device_t));
      saftedev = saftedev->next;
      safte_num++;
    }
    scsidev = scsidev->next;
  }

  return safte_num;
}


int free_saftedev(scsi_device_t *saftedev)
{
  scsi_device_t *c_saftedev = saftedev;

  while(c_saftedev->next) {
    scsi_device_t *t = c_saftedev;
    c_saftedev = c_saftedev->next;
    free(t);
  }
  return 0;
}


static char* status_str(int system, int code)
{
  safte_status_code_t *s = statuscodes;
  while(s->system) {
    if(s->system == system && s->code == code) return s->desc;
    s++;
  }
  return "unknown status";
}


static char* slot_status_str(int byte0, int byte3, int html)
{
  static char message[1024];
  safte_status_code_t *s = statuscodes;

  int first = 1;
  message[0] = '\0';
  if(byte3 == 0) {
    strcat(message, status_str(SAFTE_SLOT_BYTE3_STATUS,
			       SAFTE_SLOT_BYTE3_NOTPRESENT));
    first = 0;
  }
  while(s->system) {
    if((s->system == SAFTE_SLOT_BYTE0_STATUS && (s->code & byte0)) ||
       (s->system == SAFTE_SLOT_BYTE3_STATUS && (s->code & byte3))) {
      if(!first) {
	if(html) strcat(message, "<br>");
	else strcat(message, ",");
      }
      strcat(message, s->desc);
      first = 0;
    }
    s++;
  }
  return message;
}


static int status_severity(int system, int code)
{
  safte_status_code_t *s = statuscodes;
  while(s->system) {
    if(s->system == system && s->code == code) return s->severity;
    s++;
  }
  /* unknown status is severe */
  return 1;
}


static int slot_status_severity(int byte0, int byte3)
{
  safte_status_code_t *s = statuscodes;
  int severity = 0;

  while(s->system) {
    if(s->system == SAFTE_SLOT_BYTE0_STATUS &&
       (s->code & byte0) && s->severity > severity) severity = s->severity;
    if(s->system == SAFTE_SLOT_BYTE3_STATUS &&
       (s->code & byte3) && s->severity > severity) severity = s->severity;
    s++;
  }
  return severity;
}


static char* system_name(int system)
{

  switch(system) {
  case SAFTE_SLOT_BYTE3_STATUS:
    return "device slot";
  case SAFTE_FAN_STATUS:
    return "fan";
  case SAFTE_PSU_STATUS:
    return "power supply";
  case SAFTE_DOOR_STATUS:
    return "door lock";
  case SAFTE_SPEAKER_STATUS:
    return "speaker";
  case SAFTE_TEMP_STATUS:
    return "temp sensor";
  }

  return "unknown";
}


static char* safte_name(safte_device_t *saftedev)
{
  static char saftename[256];

  sprintf(saftename, "SAF-TE Device %s %s (%d:%d:%d:%d)",
	  saftedev->device->vendor, saftedev->device->product,
	  saftedev->device->host, saftedev->device->channel,
	  saftedev->device->id, saftedev->device->lun);

  return saftename;
}


static int run_alert_prog(safte_device_t *saftedev,
			  int system, int partno, int code, char *message)
{
  int status;
  char system_str[16];
  char code_str[16];
  char partno_str[16];

  sprintf(system_str, "%d", system);
  sprintf(partno_str, "%d", partno);
  sprintf(code_str, "%d", code);

  if(fork() == 0) {
    if(execl(alert_prog, alert_prog,
	     safte_name(saftedev), message,
	     system_str, partno_str, code_str, NULL) < 0) {
      syslog(LOG_ERR, "error exec %s: %s", alert_prog, strerror(errno));
      return -1;
    }
  } else {
    if(wait(&status) < 0) {
      syslog(LOG_ERR, "error wait: %s", strerror(errno));
    }
  }

  return 0;
}

static void log_status_alert(safte_device_t *saftedev,
			     int system, int partno, int code)
{
  char message[1024];

  if(partno == -1) sprintf(message, "%s is %s",
			   system_name(system), status_str(system, code));
  else sprintf(message, "%s %d %s",
	       system_name(system), partno, status_str(system, code));

  syslog(LOG_ALERT, "%s: ALERT %s", safte_name(saftedev), message);

  if(alert_prog) run_alert_prog(saftedev, system, partno, code, message);
}


static void log_slot_status_alert(safte_device_t *saftedev,
				  int partno, int byte0, int byte3)
{
  char message[1024];

  if(partno == -1) sprintf(message, "%s is %s",
			   system_name(SAFTE_SLOT_BYTE3_STATUS),
			   slot_status_str(byte0, byte3, 0));
  else sprintf(message, "%s %d is %s",
	       system_name(SAFTE_SLOT_BYTE3_STATUS),
	       partno, slot_status_str(byte0, byte3, 0));

  syslog(LOG_ALERT, "%s: ALERT %s", safte_name(saftedev), message);

  if(alert_prog) run_alert_prog(saftedev, SAFTE_SLOT_BYTE3_STATUS,
				partno, byte3, message);
}


static void log_temp_alert(safte_device_t *saftedev, int sensorno, float temp)
{
  char message[1024];

  sprintf(message, "temp sensor %d reads %0.1f degrees "
	  "which is %0.1f degrees over max temp of %0.1f degrees",
	  sensorno, temp, temp - max_temp, max_temp);

  syslog(LOG_ALERT, "%s: ALERT %s", safte_name(saftedev), message);

  if(alert_prog) run_alert_prog(saftedev,
				SAFTE_TEMP_STATUS, sensorno,
				SAFTE_TEMP_STATUS_ALERT, message);
}


static void log_status_change(safte_device_t *saftedev,
			      int system, int partno, int oldcode, int newcode)
{
  char message[1024];

  if(partno == -1) sprintf(message, "%s: %s changed from '%s' to '%s'",
			   safte_name(saftedev), system_name(system),
			   status_str(system, oldcode),
			   status_str(system, newcode));
  else sprintf(message, "%s: %s %d changed from '%s' to '%s'",
	       safte_name(saftedev), system_name(system), partno,
	       status_str(system, oldcode),
	       status_str(system, newcode));

  syslog(LOG_INFO, "%s", message);

  if((status_severity(system, newcode) > 0 || alert_noncrit) &&
     newcode != oldcode )
    log_status_alert(saftedev, system, partno, newcode);

}


static void log_slot_status_change(safte_device_t *saftedev, int partno,
				   int oldbyte0, int oldbyte3,
				   int newbyte0, int newbyte3)
{
  char message[1024];
  char old_slotmsg[1024];
  char new_slotmsg[1024];

  strcpy(old_slotmsg, slot_status_str(oldbyte0, oldbyte3, 0));
  strcpy(new_slotmsg, slot_status_str(newbyte0, newbyte3, 0));

  if(partno == -1) sprintf(message, "%s: %s changed from '%s' to '%s'",
			   safte_name(saftedev),
			   system_name(SAFTE_SLOT_BYTE3_STATUS),
			   old_slotmsg, new_slotmsg);
  else sprintf(message, "%s: %s %d changed from '%s' to '%s'",
	       safte_name(saftedev),
	       system_name(SAFTE_SLOT_BYTE3_STATUS), partno,
	       old_slotmsg, new_slotmsg);

  syslog(LOG_INFO, "%s", message);

  if((slot_status_severity(newbyte0, newbyte3) > 0 || alert_noncrit) &&
     (newbyte0 != oldbyte0 || newbyte3 != oldbyte3) )
    log_slot_status_alert(saftedev, partno, newbyte0, newbyte3);

}


static void log_temp_change(safte_device_t *saftedev,
			    int sensorno, float oldtemp, float newtemp)
{
  char message[1024];

  if(log_temp) {
    sprintf(message, "%s: temp sensor %d changed from "
	    "'%0.1f degrees' to '%0.1f degrees'",
	    safte_name(saftedev), sensorno, oldtemp, newtemp);

    syslog(LOG_INFO, "%s", message);
  }

  if(newtemp >= max_temp && oldtemp < max_temp)
    log_temp_alert(saftedev, sensorno, newtemp);
}


static void parse_command_line(int argc, char *argv[])
{
  int c;
  int error_flag = 0, help_flag = 0;

  while ((c = getopt(argc, argv, "hpnaNTA:t:")) != EOF)
    switch (c)
      {
      case 'p':
	print_flag++;
	break;
      case 'h':
	help_flag++;
	break;
      case 'a':
	sg_numeric = 0;
	break;
      case 'n':
	sg_numeric = 1;
	break;
      case 'N':
	alert_noncrit = 1;
	break;
      case 'T':
	log_temp = 1;
	break;
      case 'A':
	alert_prog = optarg;
	break;
      case 't':
	if(sscanf(optarg, "%f", &max_temp) != 1) {
	  error_flag++;
	  fprintf(stderr, "max temp must be a number\n");
	}
	break;
      case '?':
	error_flag++;
      }


  if (error_flag || help_flag)
    {
      fprintf(stderr, "usage:\t%s [-h] [-p] [-n] [-a] [-T] "
	      "[-t <max_temp>] [-A <alert_prog>]\n\n", argv[0]);
      fprintf(stderr,
	      "-h     show this help message\n"
	      "-p     print - print device scan information\n"
	      "-T     log temperature changes\n"
	      "-N     alert for non critical state changes\n"
	      "-A <f> program to run for alerts\n"
	      "-t <n> max temperature (default %0.1f " TEMP_UNIT ")\n"
	      "-n     numeric sg device names eg. /dev/sg0 (default)\n"
	      "-a     alpha sg device names eg. /dev/sga\n",
	      MAX_TEMP_DEFAULT);
      exit(1);
    }
}


int check_safte_status()
{
  int fd, s;
  safte_device_t *saftedev = saftedev_head;

  while(saftedev->next) {
    /* fetch safte data */
    fd = open(saftedev->device->sg_device, O_RDWR);
    if (fd < 0) {
      syslog(LOG_ERR, "open(%s): %s",
	     saftedev->device->sg_device, strerror(errno));
      exit(1);
    }
    get_safte_enclosure_status(fd, saftedev);
    get_safte_device_slot_status(fd, saftedev);
    close(fd);

    if(saftedev->copy) {
      /* compare safte data for status changes */

      /* check power supplies */
      for(s =0; s<saftedev->psus; s++)
	if(saftedev->psu[s] != saftedev->copy->psu[s])
	  log_status_change(saftedev, SAFTE_PSU_STATUS, s,
			    saftedev->copy->psu[s],
			    saftedev->psu[s]);

      /* check fans */
      for(s =0; s<saftedev->fans; s++)
	if(saftedev->fan[s] != saftedev->copy->fan[s])
	  log_status_change(saftedev, SAFTE_FAN_STATUS, s,
			    saftedev->copy->fan[s],
			    saftedev->fan[s]);

      /* check device slots */
      for(s =0; s<saftedev->slots; s++)
	if(saftedev->slot[s].status0 != saftedev->copy->slot[s].status0 ||
	   saftedev->slot[s].status3 != saftedev->copy->slot[s].status3)
	  log_slot_status_change(saftedev, s,
				 saftedev->copy->slot[s].status0,
				 saftedev->copy->slot[s].status3,
				 saftedev->slot[s].status0,
				 saftedev->slot[s].status3);

      /* check door lock */
      if(saftedev->doorlocks &&
	 saftedev->doorlock != saftedev->copy->doorlock)
	log_status_change(saftedev, SAFTE_SLOT_BYTE3_STATUS, -1,
			  saftedev->copy->doorlock,
			  saftedev->doorlock);

      /* check speaker */
      if(saftedev->audiblealarm &&
	 saftedev->speaker != saftedev->copy->speaker)
	log_status_change(saftedev, SAFTE_SPEAKER_STATUS, -1,
			  saftedev->copy->speaker,
			  saftedev->speaker);

      /* check temp sensors */
      for(s =0; s<saftedev->tempsensors; s++) {
	if(saftedev->temp[s] != saftedev->copy->temp[s])
	  log_temp_change(saftedev, s,
			  saftedev->copy->temp[s],
			  saftedev->temp[s]);
	if(saftedev->temp_oor[s] !=
	   saftedev->copy->temp_oor[s])
	  log_status_change(saftedev, SAFTE_TEMP_STATUS, s,
			    saftedev->copy->temp[s],
			    saftedev->temp[s]);
      }

      /* check overall temp alert */
      if(saftedev->temp_alert != saftedev->copy->temp_alert)
	log_status_change(saftedev, SAFTE_TEMP_STATUS, -1,
			  saftedev->copy->temp_alert,
			  saftedev->temp_alert);

    } else { 
      /* check for initial alert conditions */

      /* check power supplies */
      for(s =0; s<saftedev->psus; s++)
	if(status_severity(SAFTE_PSU_STATUS, saftedev->psu[s]) > 0
	   || alert_noncrit)
	  log_status_alert(saftedev, SAFTE_PSU_STATUS, s,
			   saftedev->psu[s]);

      /* check fans */
      for(s =0; s<saftedev->fans; s++)
	if(status_severity(SAFTE_FAN_STATUS, saftedev->fan[s]) > 0
	   || alert_noncrit)
	  log_status_alert(saftedev, SAFTE_FAN_STATUS, s,
			   saftedev->fan[s]);

      /* check device slots */
      for(s =0; s<saftedev->slots; s++)
	if(slot_status_severity(saftedev->slot[s].status0,
				saftedev->slot[s].status3) > 0
	   || alert_noncrit)
	  log_slot_status_alert(saftedev, s, saftedev->slot[s].status0,
				saftedev->slot[s].status3);

      /* check door lock */
      if(saftedev->doorlocks &&
	 (status_severity(SAFTE_DOOR_STATUS, saftedev->doorlock) > 0
	 || alert_noncrit))
	log_status_alert(saftedev, SAFTE_DOOR_STATUS, -1,
			 saftedev->doorlock);

      /* check speaker */
      if(saftedev->audiblealarm &&
	 status_severity(SAFTE_SPEAKER_STATUS,
			 saftedev->speaker) > 0)
	log_status_alert(saftedev, SAFTE_SPEAKER_STATUS, -1,
			 saftedev->speaker);

      /* check temp sensors */
      for(s =0; s<saftedev->tempsensors; s++) {
	if(saftedev->temp[s] >= max_temp
	   || alert_noncrit)
	  log_temp_alert(saftedev, s, saftedev->temp[s]);
	if(status_severity(SAFTE_TEMP_STATUS,
			   saftedev->temp_oor[s]) > 0
	   || alert_noncrit)
	  log_status_alert(saftedev, SAFTE_TEMP_STATUS, s,
			   saftedev->temp_oor[s]);
      }

      /* check overall temp alert */
      if(status_severity(SAFTE_TEMP_STATUS,
			 saftedev->temp_alert) > 0
	 || alert_noncrit)
	log_status_alert(saftedev, SAFTE_TEMP_STATUS, -1,
			 saftedev->temp_alert);
    }
		

    /* copy safte data for comparison next time around */
    if(saftedev->copy) free(saftedev->copy);
    saftedev->copy = malloc(sizeof(safte_device_t));
    memcpy(saftedev->copy, saftedev, sizeof(safte_device_t));

    saftedev = saftedev->next;
  }

  return 0;
}


static void print_safte_dev_info(FILE *out, safte_device_t *saftedev)
{
  int s;

  fprintf(out, "SAF-TE Device %s %s (%d:%d:%d:%d)\n",
	  saftedev->device->vendor, saftedev->device->product,
	  saftedev->device->host, saftedev->device->channel,
	  saftedev->device->id, saftedev->device->lun);
  fprintf(out, "no. of fans           = %d\n", saftedev->fans);
  fprintf(out, "no. of power supplies = %d\n", saftedev->psus);
  fprintf(out, "no. of device slots   = %d\n", saftedev->slots);
  fprintf(out, "door lock installed   = %d\n", saftedev->doorlocks);
  fprintf(out, "no. of temp sensors   = %d\n", saftedev->tempsensors);
  fprintf(out, "audible alarm         = %d\n", saftedev->audiblealarm);
  fprintf(out, "no. of thermostats    = %d\n", saftedev->thermostats);
  for(s =0; s<saftedev->psus; s++)
    fprintf(out, "%s %d is %s\n", system_name(SAFTE_PSU_STATUS), s,
	    status_str(SAFTE_PSU_STATUS, saftedev->psu[s]));
  for(s =0; s<saftedev->fans; s++)
    fprintf(out, "%s %d is %s\n", system_name(SAFTE_FAN_STATUS), s,
	    status_str(SAFTE_FAN_STATUS, saftedev->fan[s]));
  for(s =0; s<saftedev->slots; s++)
    fprintf(out, "%s %d %s\n", system_name(SAFTE_SLOT_BYTE3_STATUS), s,
	    slot_status_str(saftedev->slot[s].status0,
			    saftedev->slot[s].status3, 0));
  if(saftedev->doorlocks)
    fprintf(out, "%s is %s\n", system_name(SAFTE_DOOR_STATUS),
	    status_str(SAFTE_DOOR_STATUS, saftedev->doorlock));
  if(saftedev->audiblealarm)
    fprintf(out, "%s is %s\n", system_name(SAFTE_SPEAKER_STATUS),
	    status_str(SAFTE_SPEAKER_STATUS, saftedev->speaker));
  for(s =0; s<saftedev->tempsensors; s++)
    fprintf(out, "%s %d is %0.1f " TEMP_UNIT " and %s\n",
	    system_name(SAFTE_TEMP_STATUS), s, saftedev->temp[s],
	    status_str(SAFTE_TEMP_STATUS, saftedev->temp_oor[s]));
  fprintf(out, "overall temperature is %s\n",
	  status_str(SAFTE_TEMP_STATUS, saftedev->temp_alert));
  fprintf(out, "\n");
}


static void table_title(FILE *out, char *s) {
  fprintf(out, "<table border='1' cellpadding='2' cellspacing='0'>"
	  "<tr><td bgcolor='#c0c0c0' width='70' align='left' "
	  "valign='top' rowspan='2'><b>%s</b></td>", s);
}

static void table_heading(FILE *out, char *s) {
  fprintf(out, "<td bgcolor='#e0e0e0' align='center'>%s</td>", s);
}

static void table_data_start(FILE *out) {
  fprintf(out, "</tr><tr>");
}

static void table_data(FILE *out, char *s, int severity) {
  if(severity) {
    fprintf(out, "<td bgcolor='#ffa0a0' align='center'>%s</td>", s);
  } else {
    fprintf(out, "<td bgcolor='#a0ffa0' align='center'>%s</td>", s);
  }
}

static void table_data_end(FILE *out) {
  fprintf(out, "</tr></table><img src='wpixel.gif' width='1' height='3'><br>");
}

static void print_safte_dev_info_html(FILE *out, safte_device_t *saftedev)
{
  int s;
  char tmp[1024];

  fprintf(out, "<table cellpadding='0' cellspacing='0' border='0'><tr>"
	  "<td width='120' valign='top'>"
	  "<b>SAF-TE<br>Device<br>%s<br>%s<br>(%d:%d:%d:%d)</b>"
	  "</td><td>",
	  saftedev->device->vendor, saftedev->device->product,
	  saftedev->device->host, saftedev->device->channel,
	  saftedev->device->id, saftedev->device->lun);

  fprintf(out, "<table cellpadding='0' cellspacing='0' border='0'><tr><td>");
  table_title(out, "Overall");
  if(saftedev->doorlocks) table_heading(out, "Door lock");
  if(saftedev->audiblealarm) table_heading(out, "Speaker");
  table_heading(out, "Temp");
  table_data_start(out);
  if(saftedev->doorlocks) {
    sprintf(tmp, "%s\n",
	    status_str(SAFTE_DOOR_STATUS, saftedev->doorlock));
    table_data(out, tmp,
	       status_severity(SAFTE_DOOR_STATUS, saftedev->doorlock));
  }
  if(saftedev->audiblealarm) {
    sprintf(tmp, "%s\n",
	    status_str(SAFTE_SPEAKER_STATUS, saftedev->speaker));
    table_data(out, tmp,
	       status_severity(SAFTE_SPEAKER_STATUS, saftedev->speaker));
  }
  table_data(out, status_str(SAFTE_TEMP_STATUS, saftedev->temp_alert),
	     status_severity(SAFTE_TEMP_STATUS, saftedev->temp_alert));
  table_data_end(out);
  fprintf(out, "</td>");

  if(!saftedev->psus) goto temp;
  fprintf(out, "<td>&nbsp;</td><td>");
  table_title(out, "PSU");
  for(s =0; s<saftedev->psus; s++) {
    sprintf(tmp, "%d", s);
    table_heading(out, tmp);
  }
  table_data_start(out);
  for(s =0; s<saftedev->psus; s++) {
    sprintf(tmp, "%s\n", status_str(SAFTE_PSU_STATUS, saftedev->psu[s]));
    table_data(out, tmp,
	       status_severity(SAFTE_PSU_STATUS, saftedev->psu[s]));
  }
  table_data_end(out);

 temp:
  fprintf(out, "</td></tr></table>"
	  "<table cellpadding='0' cellspacing='0' border='0'><tr><td>");
  if(!saftedev->tempsensors) goto fans;
  table_title(out, "Temp");
  for(s =0; s<saftedev->tempsensors; s++) {
    sprintf(tmp, "%d", s);
    table_heading(out, tmp);
  }
  table_data_start(out);
  for(s =0; s<saftedev->tempsensors; s++) {
    sprintf(tmp, "%0.1f " TEMP_UNIT " and %s\n", saftedev->temp[s],
	    status_str(SAFTE_TEMP_STATUS, saftedev->temp_oor[s]));
    table_data(out, tmp,
	       status_severity(SAFTE_TEMP_STATUS, saftedev->temp_oor[s]));
  }
  table_data_end(out);

 fans:
  if(!saftedev->fans) goto slots;
  fprintf(out, "<td>&nbsp;</td><td>");
  table_title(out, "Fan");
  for(s =0; s<saftedev->fans; s++) {
    sprintf(tmp, "%d", s);
    table_heading(out, tmp);
  }
  table_data_start(out);
  for(s =0; s<saftedev->fans; s++) {
    sprintf(tmp, "%s\n", status_str(SAFTE_FAN_STATUS, saftedev->fan[s]));
    table_data(out, tmp,
	       status_severity(SAFTE_FAN_STATUS, saftedev->fan[s]));
  }
  table_data_end(out);

 slots:
  fprintf(out, "</td></tr></table>");
  table_title(out, "Slots");
  for(s =0; s<saftedev->slots; s++) {
    sprintf(tmp, "%d", s);
    table_heading(out, tmp);
  }
  table_data_start(out);
  for(s =0; s<saftedev->slots; s++) {
    sprintf(tmp, "%s\n", slot_status_str(saftedev->slot[s].status0,
					 saftedev->slot[s].status3, 1));
    table_data(out, tmp, slot_status_severity(saftedev->slot[s].status0,
					      saftedev->slot[s].status3));
  }
  table_data_end(out);
  fprintf(out, "</td></tr></table><br>");

}

int process_safte(struct request *r)
{
  FILE *fp;
  safte_device_t *saftedev = saftedev_head;

  char *response_hdr = "HTTP/1.1 200 OK\nContent-type: text/html\n\n"
    "<html><head><title>safte-monitor</title>"
    "<link rel='stylesheet' type='text/css' href='safte-monitor.css'>"
    "<meta http-equiv='refresh' content='10;URL=http://%s%s'>"
    "</head><body bgcolor=\"#ffffff\">";
  char *response_ftr = "</body></html>";
 
  if (r->method == M_HEAD)
    return 204;
  else if (r->method != M_GET) {
    r->error = "invalid method for safte-monitor";
    return 405;
  }

  fp = fdopen(r->cn->fd, "r+");
  fprintf(fp, response_hdr, r->servername, r->path);
  while(saftedev->next) {
    print_safte_dev_info_html(fp, saftedev);
    saftedev = saftedev->next;
  }
  fprintf(fp, "%s", response_ftr);
  fclose(fp);

  return -1;
}


int main(int argc, char **argv)
{
  int fd;
  safte_device_t *saftedev;

  parse_command_line(argc, argv);

  scsidev_head = calloc(1, sizeof(scsi_device_t));
  saftedev_head = calloc(1, sizeof(safte_device_t));
  scan_scsi_devices(sg_numeric);
  safte_num = scan_safte_devices();

  /* We can actually map SCSI disk devices to the SAF-TE device slots
     although this functionality is not presently used. It also will not
     work if you have hardware RAID as a logical device won't map to
     physical slots. */
#if 0
  map_sg_devices(TYPE_DISK, "sd", 0);
  map_slots_to_devices();
#endif

  if(print_flag) {

    if(!safte_num) {
      printf("No SAF-TE devices present\n");
      exit(0);
    } else {
      printf("Found %d SAF-TE devices\n", safte_num);
    }

    saftedev = saftedev_head;
    while(saftedev->next) {
      fd = open(saftedev->device->sg_device, O_RDWR);
      if (fd < 0) {
	perror("open");
	exit(1);
      }
      get_safte_enclosure_status(fd, saftedev);
      get_safte_device_slot_status(fd, saftedev);
      close(fd);

      print_safte_dev_info(stdout, saftedev);
      saftedev = saftedev->next;
    }
    exit(0);
  }

  /* background mode */
  openlog("safte-monitor", LOG_PID, LOG_DAEMON);

  mathopd_main(argc, argv);

  closelog();
  exit(0);
}


