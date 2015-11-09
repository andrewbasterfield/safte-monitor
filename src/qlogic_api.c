#include <stdio.h>
#include <stdlib.h>

#include "scsi_api.h"
#include "qlogic_api.h"

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


static fc_device_t *qldev_head;

int qlogic_probe(int fd)
{
    QL_IOCTL ql;
    QL_DISC_PORT qlport;
    QL_SCSI_ADDR qlscsi;
    int ql_num, i, j, k;
    fc_device_t *qldev;

    if(qldev_head) return; /* already probed */

    if(msizeof(QL_IOCTL, Signature) == 4)
      strcpy((char*)&ql.Signature, "QLOG");
    else if(msizeof(QL_IOCTL, Signature) == 8)
      strcpy((char*)&ql.Signature, "QLOGIC");
    ql.Version = QL_VERSION;
    if(ioctl(fd, QL_IOCTL_STARTIOCTL, &ql) < 0) {
      fprintf(stderr, "qlogic_probe: QL_IOCTL_STARTIOCTL failed\n");
      return;
    }

    if(!qldev_head) qldev = qldev_head = calloc(1, sizeof(fc_device_t));

    ql_num =  ql.Instance;
    for(i=0; i<ql_num && i < 5; i++) {
      ql.Instance = i;
      if(ioctl(fd, QL_IOCTL_SETINSTANCE, &ql) < 0) {
	fprintf(stderr, "qlogic_probe: QL_IOCTL_SETINSTANCE failed\n");
	return;
      }
      for(j=0; j < QL_MAX_FIBRE_DEVICES; j++) {
	ql.SubCode = QL_IOCTL_SC_QUERY_DISC_PORT;
	ql.ResponseAdr = &qlport;
	ql.ResponseLen = sizeof(qlport);
	ql.Instance = j;
	if(ioctl(fd, QL_IOCTL_QUERY, &ql) >= 0) {
	  if(ql.Status == QL_STATUS_DEV_NOT_FOUND ||
	     ql.DetailStatus == QL_STATUS_DEV_NOT_FOUND) break;
	  ql.RequestAdr = qlport.WWPN;
	  ql.RequestLen = QL_DEF_WWN_NAME_SIZE;
	  ql.ResponseAdr = &qlscsi;
	  ql.ResponseLen = sizeof(qlscsi);
	  if(ioctl(fd, QL_IOCTL_WWPN_TO_SCSIADDR, &ql) >= 0) {
#ifdef DEBUG
	  printf("host=%d wwn=%02x%02x%02x%02x%02x%02x%02x%02x id=%d\n",
		 ql.HbaSelect,
		 qlport.WWPN[0], qlport.WWPN[1], qlport.WWPN[2],
		 qlport.WWPN[3], qlport.WWPN[4], qlport.WWPN[5],
		 qlport.WWPN[6], qlport.WWPN[7], qlscsi.Target);
#endif
	  sprintf(qldev->wwpn, "%02x%02x%02x%02x%02x%02x%02x%02x",
		  qlport.WWPN[0], qlport.WWPN[1], qlport.WWPN[2],
		  qlport.WWPN[3], qlport.WWPN[4], qlport.WWPN[5],
		  qlport.WWPN[6], qlport.WWPN[7]);
	  sprintf(qldev->wwnn, "%02x%02x%02x%02x%02x%02x%02x%02x",
		  qlport.WWNN[0], qlport.WWNN[1], qlport.WWNN[2],
		  qlport.WWNN[3], qlport.WWNN[4], qlport.WWNN[5],
		  qlport.WWNN[6], qlport.WWNN[7]);
	  sprintf(qldev->portid, "%02x%02x%02x",
		  qlport.Id[1], qlport.Id[2], qlport.Id[3]);
	  qldev->loopid = qlport.LoopID;
	  qldev->host = ql.HbaSelect;
	  qldev->id = qlscsi.Target;
	  qldev->next = calloc(1, sizeof(fc_device_t));
	  qldev = qldev->next;

	  }
	}
      }
    }

}

int qlogic_map_wwn_to_sd()
{
    fc_device_t *qldev = qldev_head; 
    scsi_device_t *scsidev;
    int lun;

    if(!qldev) return;
    while(qldev->next) {
      for(lun=0; lun<256; lun++) {
	/* all luns have the same WWPN */
	if(scsidev = find_dev_by_loc(qldev->host, 0, qldev->id, lun)) {
	  scsidev->isfc = 1;
	  strcpy(scsidev->wwpn, qldev->wwpn);
	  strcpy(scsidev->wwnn, qldev->wwnn);
	  strcpy(scsidev->portid, qldev->portid);
	  scsidev->loopid = qldev->loopid;
	}
      }
      qldev = qldev->next;
    }
    
}

void qlogic_free()
{
    fc_device_t *qldev = qldev_head; 

    if(!qldev) return;
    while(qldev->next) {
      fc_device_t *t = qldev;
      qldev = qldev->next;
      free(t);
    }
    qldev_head = NULL;
}
