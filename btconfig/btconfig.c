/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h> 
#include <time.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <endian.h>
#include <byteswap.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cutils/properties.h>

#ifdef ANDROID
#include <termios.h>
#else
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <limits.h>
#endif



#include "btconfig.h"
#include "masterblaster.h"

#define PRONTO_SOC TRUE
#define Inquiry_Complete_Event  0x01

void baswap(bdaddr_t *dst, const bdaddr_t *src)
{
	register unsigned char *d = (unsigned char *) dst;
	register const unsigned char *s = (const unsigned char *) src;
	register int i;

	for (i = 0; i < 6; i++)
		d[i] = s[5-i];
}

int bachk(const char *str)
{
	if (!str)
		return -1;

	if (strlen(str) != 17)
		return -1;

	while (*str) {
		if (!isxdigit(*str++))
			return -1;

		if (!isxdigit(*str++))
			return -1;

		if (*str == 0)
			break;

		if (*str++ != ':')
			return -1;
	}

	return 0;
}

int ba2str(const bdaddr_t *ba, char *str)
{
	return snprintf(str, strlen(str)+1,"%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
		ba->b[5], ba->b[4], ba->b[3], ba->b[2], ba->b[1], ba->b[0]);
}

int str2ba(const char *str, bdaddr_t *ba)
{
	bdaddr_t b;
	int i;

	if (bachk(str) < 0) {
		memset(ba, 0, sizeof(*ba));
		return -1;
	}

	for (i = 0; i < 6; i++, str += 3)
		b.b[i] = strtol(str, NULL, 16);

	baswap(ba, &b);

	return 0;
}

/* Redefine a small buffer for our simple text config files */
#undef BUFSIZ
#define BUFSIZ 128

ssize_t
getline(char ** __restrict buf, size_t * __restrict buflen,
    FILE * __restrict fp)
{
        size_t bytes, newlen;
        char *newbuf, *p;

        if (buf == NULL || buflen == NULL) {
                errno = EINVAL;
                return -1;
        }
        if (*buf == NULL)
                *buflen = 0;

        bytes = 0;
        do {
                if (feof(fp))
                        break;
                if (*buf == NULL || bytes != 0) {
                        newlen = *buflen + BUFSIZ;
                        newbuf = realloc(*buf, newlen);
                        if (newbuf == NULL)
                                return -1;
                        *buf = newbuf;
                        *buflen = newlen;
                }
                p = *buf + bytes;
                memset(p, 0, BUFSIZ);
                if (fgets(p, BUFSIZ, fp) == NULL)
                        break;
                bytes += strlen(p);
        } while (bytes == 0 || *(*buf + (bytes - 1)) != '\n');
        if (bytes == 0)
                return -1;
        return bytes;
}

/* Global Variables */
static int Tag_Count = 0;
static int Patch_Count = 0;
static unsigned short DynMem_Count = 0;
static int Total_tag_lenght = 0;
static BOOL CtrlCBreak = FALSE;
bdaddr_t BdAddr;
/* Function Declarations */
static void LoadPSHeader(UCHAR *HCI_PS_Command,UCHAR opcode,int length,int index);
static BOOL PSOperations(int uart_fd, UCHAR Opcode, int Param1, UINT32 *out);
static BOOL SU_LERxTest(int uart_fd, UCHAR channel);
static BOOL SU_LETxTest(int uart_fd, UCHAR channel, UCHAR length, UCHAR payload);
static void usage(void);
static int writeHciCommand(int uart_fd, uint16_t ogf, uint16_t ocf, uint8_t plen, UCHAR *buf);
static int MemBlkRead(int uart_fd, UINT32 Address,UCHAR *pBuffer, UINT32 Length);
static int MemBlkwrite(int uart_fd, UINT32 Address,UCHAR *pBuffer, UINT32 Length);
static int Dut(int uart_fd);
static int ReadAudioStats(int uart_fd);
static int ReadGlobalDMAStats(int uart_fd);
static int ResetGlobalDMAStats(int uart_fd);
static int ReadTpcTable(int uart_fd);
static int ReadHostInterest(int uart_fd,tBtHostInterest *pHostInt);
static int ReadMemoryBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length );
static int WriteMemoryBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length );
static int write_otpRaw(int uart_fd, int address, int length, UCHAR *data);
static int read_otpRaw(int uart_fd, int address, int length, UCHAR *data);
static void dumpHex(UCHAR *buf, int length, int col);
static void sig_term(int sig);
static UCHAR LEMode = 0;

int read_hci_event(int fd, unsigned char* buf, int size);
int set_speed(int fd, struct termios *ti, int speed);

static struct option main_options[] = {
	{ "help",	0, 0, 'h' },
	{ "device",	1, 0, 'i' },
	{ 0, 0, 0, 0 }
};

unsigned int uGetInputDataFormat(char **str, struct ST_PS_DATA_FORMAT *pstFormat)
{
	char *pCharLine = *str;
	if(pCharLine[0] != '[') {
		pstFormat->eDataType = eHex;
		pstFormat->bIsArray = TRUE;
		return TRUE;
	}
	switch(pCharLine[1]) {
		case 'H':
		case 'h':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'a') || (pCharLine[3]== 'A')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[H:A
					return FALSE;
				}
			}
			if((pCharLine[3]== 'S') || (pCharLine[3]== 's')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = FALSE;
					//pCharLine += 5;
					*str += 5;
					//printf("\nDEBUG H-1:%s\n",pCharLine);
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[H:A
					return FALSE;
				}
			}
			else if(pCharLine[3] == ']') {         //[H:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 4;
				return TRUE;
			}
			else {                            //[H:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;
			}
		}
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;
		}
		break;

		case 'A':
		case 'a':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'h') || (pCharLine[3]== 'H')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n"); //[A:H
					return FALSE;
				}
			}
			else if(pCharLine[3]== ']') {         //[A:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 5;
				return TRUE;
			}
			else {                            //[A:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;
			}
        }
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;

		}
		break;

		case 'S':
		case 's':
        if(pCharLine[2]==':') {
			if((pCharLine[3]== 'h') || (pCharLine[3]== 'H')) {
				if(pCharLine[4] == ']') {
					pstFormat->eDataType = eHex;
					pstFormat->bIsArray = TRUE;
					//pCharLine += 5;
					*str += 5;
					return TRUE;
				}
				else {
					printf("\nuGetInputDataFormat - Invalid Data Format \r\n");//[A:H
					return FALSE;
				}
			}
			else if(pCharLine[3]== ']') {         //[A:]
				pstFormat->eDataType = eHex;
				pstFormat->bIsArray = TRUE;
				//pCharLine += 4;
				*str += 5;
				return TRUE;
			}
			else {                            //[A:
				printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
				return FALSE;
			}
		}
		else if(pCharLine[2]==']') {    //[H]
			pstFormat->eDataType = eHex;
			pstFormat->bIsArray = TRUE;
			//pCharLine += 3;
			*str += 5;
			return TRUE;
		}
		else {                      //[H
			printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
			return FALSE;
		}
		break;

		default:
		printf("\nuGetInputDataFormat - Invalid Data Format \r\n");
		return FALSE;
	}
}

unsigned int uReadDataInSection(char *pCharLine, struct ST_PS_DATA_FORMAT stPS_DataFormat)
{
	if(stPS_DataFormat.eDataType == eHex) {
		if(stPS_DataFormat.bIsArray == TRUE) {
            //Not implemented
			printf("\nNO IMP\n");
            return (0x0FFF);
		}
		else {
			//printf("\nDEBUG H-2 %d\n",strtol(pCharLine, NULL, 16));
			return (strtol(pCharLine, NULL, 16));
		}
	}
	else {
        //Not implemented
			printf("\nNO IMP-1\n");
			return (0x0FFF);
	}
}

static void LoadPSHeader(UCHAR *HCI_PS_Command,UCHAR opcode,int length,int index) {

	HCI_PS_Command[0]= 0x01;
	HCI_PS_Command[1]= 0x02;
	HCI_PS_Command[2]= BD_ADDR_SIZE;	

}


static BOOL PSOperations(int uart_fd, UCHAR Opcode, int Param1, UINT32 *out) {
	UCHAR buf[MAX_EVENT_SIZE];
	int Length,i,j,iRet;
	memset(&buf,0,sizeof(buf));
	switch(Opcode){
		case WRITE_PATCH:
			for(i=0;i< Param1;i++){
				LoadPSHeader(buf,Opcode,RamPatch[i].Len,i);
				for(j=0;j<RamPatch[i].Len;j++){
					buf[4+j]=RamPatch[i].Data[j];
				}
				iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_PS,RamPatch[i].Len + PS_COMMAND_HEADER, buf);
				if(buf[iRet-1] != 0){
					return FALSE;
				}
			}
			break;

		case ENABLE_PATCH:
			Length =0;
			i=0;
			LoadPSHeader(buf,Opcode,Length,i);
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_RESET:
			Length =0;
			i=0;
			LoadPSHeader(buf,Opcode,Length,i);
			buf[8] = (Param1 & 0xFF);
			buf[9] = ((Param1 >>  8) & 0xFF);
			Length = 6;
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,Length + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_READ: {
			UCHAR *len = (UCHAR *)out;
			ssize_t plen = 0;
			Length = len[0] | ( len[1] << 8);
			LoadPSHeader(buf,Opcode,Length,Param1);
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS, Length + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0) {
				return FALSE;
			}
			do {
				plen = read(uart_fd, buf, MAX_EVENT_SIZE);
				if (plen < 0)
					return FALSE;
			} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
			memcpy((UCHAR *)out, buf + HCI_EVENT_HEADER_SIZE + 1, plen - HCI_EVENT_HEADER_SIZE - 1);
			break;
		}

		case PS_WRITE:
			for(i=0;i< Param1;i++){
				LoadPSHeader(buf,Opcode,PsTagEntry[i].TagLen,PsTagEntry[i].TagId);
				for(j=0;j<PsTagEntry[i].TagLen;j++){
					buf[4+j]=PsTagEntry[i].TagData[j];
				}
				iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PsTagEntry[i].TagLen + PS_COMMAND_HEADER, buf);
				if(buf[iRet-1] != 0){
					return FALSE;
				}
			}
			break;

		case PS_DYNMEM_OVERRIDE:
			LoadPSHeader(buf,Opcode,RamDynMemOverride.Len,RamDynMemOverride.Len);
			for(j=0;j<RamDynMemOverride.Len;j++){
				buf[4+j]=RamDynMemOverride.Data[j];
			}
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_PS,RamDynMemOverride.Len + PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_VERIFY_CRC:
			//printf("PSOperations - PS_VERIFY_CRC:VALUE of CRC:%d\r\n",Param1);
			Length =0;
			LoadPSHeader(buf,Opcode,Length,Param1);
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS, PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			break;

		case PS_GET_LENGTH: {
			ssize_t plen = 0;
			LoadPSHeader(buf,Opcode,0,Param1);
			iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS, PS_COMMAND_HEADER, buf);
			if(buf[iRet-1] != 0){
				return FALSE;
			}
			do {
				plen = read(uart_fd, buf, MAX_EVENT_SIZE);
				if (plen < 0)
					return FALSE;
			} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
			*((UINT16 *)out) = (buf[HCI_EVENT_HEADER_SIZE + 2] << 8) | buf[HCI_EVENT_HEADER_SIZE + 1];
			break;
		}
	}
		return TRUE;
}

/* HCI functions that require open device
 * dd - Device descriptor returned by hci_open_dev. */

int hci_send_cmd(int uart_fd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
	uint8_t type = 0x01; //HCI_COMMAND_PKT
	uint8_t hci_command_buf[256] = {0};
	uint16_t opcode;
	int i;

	opcode = htobs(HCI_OPCODE_PACK(ogf, ocf));

	//hci_command_buf[0]= type;
	hci_command_buf[0]= (uint8_t)(opcode & 0xff	);
	hci_command_buf[1]= (uint8_t)((opcode>>8) & 0xff);
	hci_command_buf[2]= plen;

	if(plen) {
		memcpy(&hci_command_buf[3], (uint8_t*) param, plen);
	}

	if( write(uart_fd, hci_command_buf, plen+3) < 0) {
		return -1;
	}
	return 0;
}

/*
 * Read an HCI event from the given file descriptor.
 */
int read_hci_event(int fd, unsigned char* buf, int size)
{
	int remain, r, i;
	int count = 0;

	if (size <= 0)
		return -1;

	/* The first byte identifies the packet type. For HCI event packets, it
	 * should be 0x04, so we read until we get to the 0x04. */
	while (1) {
		r = read(fd, buf, 1);
		if (r <= 0)
			return -1;
		if (buf[0] == 0x04) {
			printf("pkt: %d\n", buf[0]);
			break;
		}
	}
	count++;

	/* The next two bytes are the event code and parameter total length. */
	while (count < 3) {
		r = read(fd, buf + count, 3 - count);
		if (r <= 0)
			return -1;
		count += r;
	}
	for (i=1 ; i<3 ; i++)
		printf("next pkt: %02x\n", buf[i]);

	/* Now we read the parameters. */
	if (buf[2] < (size - 3))
		remain = buf[2];
	else
		remain = size - 3;

	while ((count - 3) < remain) {
		r = read(fd, buf + count, remain - (count - 3));
		if (r <= 0)
			return -1;
		count += r;
	}

	for (i=3 ; i<r ; i++)
		printf("next pkts: %d\n", buf[i]);


	return count;
}
int read_event_modified(int fd, unsigned char* buf, int to)
{
	int remain, r,i,size;
	int count = 0;


	//do{
		/* The first byte identifies the packet type. For HCI event packets, it
		 * should be 0x04, so we read until we get to the 0x04. */
#if 0
		while (1) {
			r = read(fd, buf, 1);
			if (r <= 0)
				return -1;
			if (buf[0] == 0x04)
				break;
		}
		count++;

#endif		/* The next two bytes are the event code and parameter total length. */
		while (count < 2) {
			r = read(fd, buf + count, 2 - count);
			if (r <= 0)
				return -1;
			count += r;
		}

		if (buf[1] == 0)
		{ 
			printf("Zero len , invalid \n");
			return -1;
		}
		size = buf[1]+2; 

		/* Now we read the parameters. */
		while (count  < size ) {
			r = read(fd, buf + count, size);
			if (r <= 0)
			{
				printf("read error \n");
				return -1;
			}
			count += r;
		}

		printf("\n*************Data read begin ************* \n");
		for (i=0 ; i< buf[1] + 2 ; i++)
			printf("[%x]", buf[i]);
		printf("\n*************Data read end **************\n");

		/* buf[1] should be the event opcode 
		 * buf[2] shoudl be the parameter Len of the event opcode 
		 */
	//}while (buf[2] != Inquiry_Complete_Event);

	return count;
}
int read_incoming_events(int fd, unsigned char* buf, int to)
{
	int remain, r,i,size;
	int count = 0;
	time_t start, finish, checkbreak;
	struct timeval tv1, tv2, timeout;
	unsigned long long start_utime, end_utime, time_diff;
	unsigned long long diff;
	
	start = time(NULL);
	gettimeofday(&tv1, NULL);
	start_utime = tv1.tv_sec*1000000 + tv1.tv_usec;

	do{
		/* The first byte identifies the packet type. For HCI event packets, it
		 * should be 0x04, so we read until we get to the 0x04. */
		/* The next two bytes are the event code and parameter total length. */
		while (count < 2) {
			r = read(fd, buf + count, 2 - count);
			if (r <= 0)
                        {
                             
				printf("read error \n");
				return -1;
                        }
			count += r;
		}


		if (buf[1] == 0)
		{ 
			printf("Zero len , invalid \n");
			return -1;
		}
		size = buf[1]; 

		/* Now we read the parameters. */
		while (count  < size ) {
			r = read(fd, buf + count, size);
			if (r <= 0)
			{
				printf("read error :size = %d\n",size);
				return -1;
			}
			count += r;
		}

		switch (buf[0])
		{
			int j=0;
			case 0x0f:
				printf("Command Status Received\n");
				for (j=0 ; j< buf[1] + 2 ; j++)
					printf("[%x]", buf[j]);
				printf("\n");
                                if(buf[2] == 0x02)
                                {
                                    printf("\nUnknown connection identifier");
                                    return 0;
                                }     
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				break;

			case 0x02:
				printf("INQ RESULT EVENT RECEIVED \n");
				for (j=0 ; j< buf[1] + 2 ; j++)
					printf("[%x]", buf[j]);
				printf("\n");
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				break;
			case 0x01:
				printf("INQ COMPLETE EVENT RECEIVED\n");
 
				printf("\n");
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				return 0;
			case 0x03:
                                if(buf[2] == 0x00)
					printf("CONNECTION COMPLETE EVENT RECEIVED WITH HANDLE: 0x%02x%02x \n",buf[4],buf[3]);
                                else
					printf("CONNECTION COMPLETE EVENT RECEIVED WITH ERROR CODE 0x%x \n",buf[2]);
				for (j=0 ; j< buf[1] + 2 ; j++)
					printf("[%x]", buf[j]);
				printf("\n");
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				return 0;
			case 0x05:
				printf("DISCONNECTION COMPLETE EVENT RECEIVED WITH REASON CODE: 0x%x \n",buf[5]);
				for (j=0 ; j< buf[1] + 2 ; j++)
					printf("[%x]", buf[j]);
				printf("\n");
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				return 0;
			default:
				printf("Other event received, Breaking\n");
				for (j=0 ; j< buf[1] + 2 ; j++)
					printf("[%x]", buf[j]);
				printf("\n");
				memset(buf , 0, sizeof(buf));
				count = 0; i = 0;size =0;
				break;
				
		}
	
		/* buf[1] should be the event opcode 
		 * buf[2] shoudl be the parameter Len of the event opcode 
		 */
	}while (1);
	return count;
}
static int writeHciCommand(int uart_fd, uint16_t ogf, uint16_t ocf, uint8_t plen, UCHAR *buf){
#ifdef DUMP_DEBUG
	int i = 0, j = 0;
	uint16_t opcode = htobs(HCI_OPCODE_PACK(ogf, ocf));

	printf("\nDump:\n");
	printf("0x%02X ", opcode & 0xff);
	i++;
	printf("0x%02X ", (opcode & 0xff00) >> 8);
	i++;
	printf("0x%02X ", plen);
	i++;
	for (j = 0; j < plen; i++, j++) {
		printf("0x%02X ", buf[j]);
		if (((i+1) % 8) == 0 && i != 0)
			printf("\n");
	}
	if (((i+1) % 8) != 0) printf("\n");
	buf[6] = 0;
	return plen;
#else
	//printf("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d\n", ogf, ocf, plen);
	if (hci_send_cmd(uart_fd, ogf, ocf, plen, buf) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}
	sleep(0.4);

	return read_event_modified(uart_fd, buf, 0);

	//return read_hci_event(uart_fd, buf, MAX_EVENT_SIZE);

#endif
}
static int MemBlkRead(int uart_fd,UINT32 Address,UCHAR *pBuffer, UINT32 Length){
	UINT32         Size, ByteLeft,IntCnt;
	UCHAR          *pData,*pTemp=pBuffer;
	int iRet;
	int TempVal;

	IntCnt =0;
	TempVal = (Length % 4);
	if (TempVal !=0) {
		Length = Length + (4- (Length%4));
	}
	ByteLeft = Length;
	while (ByteLeft > 0)
	{
		Size = (ByteLeft > MEM_BLK_DATA_MAX) ? MEM_BLK_DATA_MAX : ByteLeft;
	//	printf("\nMemBlkwrite : Size :%x   Address :%x\n",Size, Address);
		pData = (UCHAR *) malloc(Size + 6);
		pData[0]= 0x00;//depot/esw/projects/azure/AR3001_3_0/src/hci/Hci_Vsc_Proc.c
		pData[1]= (Address & 0xFF);
		pData[2]= ((Address >> 8) & 0xFF);
		pData[3]= ((Address >> 16) & 0xFF);
		pData[4]= ((Address >> 24) & 0xFF);
		pData[5]= Size;
		iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_MEMOP,6,pData);
		if(pData[6]!= 0){
			printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);
			free(pData);
			return FALSE;
		}
		if ((read(uart_fd, pData,MAX_EVENT_SIZE)) < 0) {
			perror("Read failed");
			exit(EXIT_FAILURE);
			}

			if(pData[3]!=3) {
				perror("Read failed");
				exit(EXIT_FAILURE);
			}
			memcpy(pTemp,(pData+4),Size);
			pTemp+=Size;
			IntCnt = Size;
			ByteLeft -= Size;
			Address += Size;
			free(pData);
	}
	return TRUE;
}


static int MemBlkwrite(int uart_fd,UINT32 Address,UCHAR *pBuffer, UINT32 Length){
	UINT32         Size, ByteLeft,IntCnt;
	UCHAR          *pData;
	int iRet;
	ByteLeft = Length;
	IntCnt =0;
	while (ByteLeft > 0)
	{
		Size = (ByteLeft > MEM_BLK_DATA_MAX) ? MEM_BLK_DATA_MAX : ByteLeft;
	//	printf("\nMemBlkwrite : Size :%x   Address :%x\n",Size, Address);
		pData = (UCHAR *) malloc(Size + 6);
		pData[0]= 0x01;
		pData[1]= (Address & 0xFF);
		pData[2]= ((Address >> 8) & 0xFF);
		pData[3]= ((Address >> 16) & 0xFF);
		pData[4]= ((Address >> 24) & 0xFF);
		pData[5]= Size;
		memcpy(&pData[6],&pBuffer[IntCnt],Size);
		iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_MEMOP,Size+6,pData);
		if(pData[6]!= 0){
			printf("\nwrite memory command faileddue to reason 0x%X\n",pData[6]);
			free(pData);
			return FALSE;
		}
		IntCnt = Size;
		ByteLeft -= Size;
		Address += Size;
		free(pData);
	}
	return TRUE;
}

static int Dut(int uart_fd){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];


	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 3; //All scan enabled
	// OGF_HOST_CTL
	// OCF_WRITE_SCAN_ENABLE
	iRet = writeHciCommand(uart_fd, 0x03, 0x001A, 1, buf);
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	sleep(1);


	memset(&buf,0,MAX_EVENT_SIZE);
	//OGF_TEST_CMD
	//OCF_ENABLE_DEVICE_UNDER_TEST_MODE
	iRet = writeHciCommand(uart_fd, 0x06, 0x0003, 0, buf);
	if(buf[6] != 0){
		printf("\nDUT mode command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
#ifndef PRONTO_SOC
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0; //SEQN Track enable =0
	// HCI_VENDOR_CMD_OGF
	// OCF_TEST_MODE_SEQN_TRACKING
	iRet = writeHciCommand(uart_fd, 0x3F, 0x0018, 1, buf);
	if(buf[6] != 0){
		printf("\nTest Mode seqn Tracking failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
#endif
	return TRUE;
}


void Audio_DumpStats(tAudioStat *AudioStats)
{
	printf("\n\n");
	printf("    Audio Statistics\n");
	printf(">RxCmplt:                 %d\n",AudioStats->RxCmplt);
	printf(">TxCmplt:                 %d\n",AudioStats->TxCmplt);
	printf(">RxSilenceInsert:         %d\n",AudioStats->RxSilenceInsert);
	printf(">RxAirPktDump:            %d\n",AudioStats->RxAirPktDump);
	printf(">MaxPLCGenInterval:       %d\n",AudioStats->MaxPLCGenInterval);
	printf(">RxAirPktStatusGood:      %d\n",AudioStats->RxAirPktStatusGood);
	printf(">RxAirPktStatusError:     %d\n",AudioStats->RxAirPktStatusError);
	printf(">RxAirPktStatusLost:      %d\n",AudioStats->RxAirPktStatusLost);
	printf(">RxAirPktStatusPartial:   %d\n",AudioStats->RxAirPktStatusPartial);
	printf(">SampleMin:               %d\n",AudioStats->SampleMin);
	printf(">SampleMax:               %d\n",AudioStats->SampleMax);
	printf(">SampleCounter:           %d\n",AudioStats->SampleCounter);
	printf("\n\n");

	memset((UCHAR *)AudioStats, 0, sizeof(tAudioStat));
	AudioStats->SampleMax =SHRT_MIN;
	AudioStats->SampleMin =SHRT_MAX;
}

static int ReadAudioStats(int uart_fd){

	tBtHostInterest HostInt;
	tAudioStat Stats;

	ReadHostInterest(uart_fd, &HostInt);
	if(!HostInt.AudioStatAddr || (HostInt.Version < 0x0300)){
		printf("\nAudio Stat not present\n");
		return FALSE;
	}
	ReadMemoryBlock(uart_fd,HostInt.AudioStatAddr,(UCHAR *)&Stats,sizeof(tAudioStat));
	Audio_DumpStats(&Stats);
	return TRUE;
}

void BRM_DumpStats(tBRM_Stats *Stats)
{
   printf("\n  Link Controller Voice DMA Statistics\n");
   printf("  %22s: %u\n", "VoiceTxDmaIntrs", Stats->VoiceTxDmaIntrs);
   printf("  %22s: %u\n", "VoiceTxPktAvail", Stats->VoiceTxPktAvail);
   printf("  %22s: %u\n", "VoiceTxPktDumped", Stats->VoiceTxPktDumped);
   printf("  %22s: %u\n", "VoiceTxErrors", Stats->VoiceTxErrorIntrs);
   printf("  %22s: %u\n", "VoiceTxDmaErrors", Stats->VoiceTxDmaErrorIntrs);
   printf("  %22s: %u\n", "VoiceTxSilenceInserts", Stats->VoiceTxDmaSilenceInserts);
   printf("\n");
   printf("  %22s: %u\n", "VoiceRxDmaIntrs", Stats->VoiceRxDmaIntrs);
   printf("  %22s: %u\n", "VoiceRxGoodPkts", Stats->VoiceRxGoodPkts);
   printf("  %22s: %u\n", "VoiceRxPktDumped", Stats->VoiceRxPktDumped);
   printf("  %22s: %u\n", "VoiceRxErrors", Stats->VoiceRxErrorIntrs);
   printf("  %22s: %u\n", "VoiceRxCRC", Stats->VoiceRxErrCrc);
   printf("  %22s: %u\n", "VoiceRxUnderOverFlow", Stats->VoiceRxErrUnderOverFlow);
   printf("\n");
   printf("  %22s: %u\n", "SchedOnVoiceError", Stats->SchedOnVoiceError);
   printf("  %22s: %u\n", "VoiceTxReapOnError", Stats->VoiceTxReapOnError);
   printf("  %22s: %u\n", "VoiceRxReapOnError", Stats->VoiceRxReapOnError);
   printf("  %22s: %u\n", "VoiceSchedulingError", Stats->VoiceSchedulingError);

   printf("\n  Link Controller ACL DMA Statistics\n");
   printf("  %22s: %u\n", "DmaIntrs", Stats->DmaIntrs);
   printf("  %22s: %u\n", "ErrWrongLlid", Stats->ErrWrongLlid);
   printf("  %22s: %u\n", "ErrL2CapLen", Stats->ErrL2CapLen);
   printf("  %22s: %u\n", "ErrUnderOverFlow", Stats->ErrUnderOverFlow);
   printf("  %22s: %u\n", "RxBufferDumped", Stats->RxBufferDumped);
   printf("  %22s: %u\n", "ErrWrongLmpPktType", Stats->ErrWrongLmpPktType);
   printf("  %22s: %u\n", "ErrWrongL2CapPktType", Stats->ErrWrongL2CapPktType);
   printf("  %22s: %u\n", "IgnoredPkts", Stats->IgnoredPkts);
   printf("\n");
   printf("  %22s: %u\n", "Data TxBuffers", Stats->DataTxBuffers);
   printf("  %22s: %u\n", "Data RxBuffers", Stats->DataRxBuffers);
   printf("  %22s: %u\n", "LMP TxBuffers", Stats->LmpTxBuffers);
   printf("  %22s: %u\n", "LMP RxBuffers", Stats->LmpRxBuffers);
   printf("  %22s: %u\n", "HEC Errors", Stats->HecFailPkts);
   printf("  %22s: %u\n", "CRC Errors", Stats->CrcFailPkts);

   // Buffer Management
   printf("\n  Buffer Management Statistics\n");
   printf("  %22s: %u\n", "CtrlErrNoLmpBufs", Stats->CtrlErrNoLmpBufs);

   printf("\n  Sniff Statistics\n");
   printf("  %22s: %u\n", "SniffSchedulingError", Stats->SniffSchedulingError);
   printf("  %22s: %u\n", "SniffIntervalNoCorr", Stats->SniffIntervalNoCorr);

   // Other stats
   printf("\n  Other Statistics\n");
   printf("  %22s: %u\n", "ForceOverQosJob", Stats->ForceOverQosJob);
   //printf("  %22s: %u\n", "Temp 1", Stats->Temp1);
   //printf("  %22s: %u\n", "Temp 2", Stats->Temp2);

   // Test Mode Stats
   printf("\n  Test Mode Statistics\n");
   printf("  %22s: %u\n", "TestModeDroppedTxPkts", Stats->TestModeDroppedTxPkts);
   printf("  %22s: %u\n", "TestModeDroppedLmps", Stats->TestModeDroppedLmps);

   // Error Stats
   printf("\n  General Error Statistics\n");
   printf("  %22s: %u\n", "TimePassedIntrs", Stats->TimePassedIntrs);
   printf("  %22s: %u\n", "NoCommandIntrs", Stats->NoCommandIntrs);
}

static int ReadGlobalDMAStats(int uart_fd){
	tBtHostInterest	HostInt;
	tBRM_Stats  Stats;

	ReadHostInterest(uart_fd, &HostInt);
	if(!HostInt.GlobalDmaStats || (HostInt.Version < 0x0100)){
		printf("\nGlobal DMA stats not present\n");
		return FALSE;
	}
	ReadMemoryBlock(uart_fd,HostInt.GlobalDmaStats,(UCHAR *)&Stats,sizeof(tBRM_Stats));
	BRM_DumpStats(&Stats);
	return TRUE;
}

static int ResetGlobalDMAStats(int uart_fd){
	tBtHostInterest	HostInt;
	tBRM_Stats  Stats;

	ReadHostInterest(uart_fd, &HostInt);
	if(!HostInt.GlobalDmaStats || (HostInt.Version < 0x0100)){
		printf("\nGlobal DMA stats not present\n");
		return FALSE;
	}
	memset(&Stats,0,sizeof(Stats));
	printf("\nHarry\n");
	WriteMemoryBlock(uart_fd,HostInt.GlobalDmaStats,(UCHAR *)&Stats,sizeof(tBRM_Stats));
	printf("\nDMA stattestics has been reset\n");
	return TRUE;
}

static int ReadTpcTable(int uart_fd){
	tBtHostInterest	HostInt;
	tPsSysCfgTransmitPowerControlTable  TpcTable;
	int i;

	ReadHostInterest(uart_fd, &HostInt);
	if(!HostInt.TpcTableAddr || (HostInt.Version < 0x0100)){
		printf("\nTPC table not present\n");
		return FALSE;
	}
	ReadMemoryBlock(uart_fd,HostInt.TpcTableAddr,(UCHAR *)&TpcTable,sizeof(TpcTable));
	for(i=0;i< TpcTable.NumOfEntries; i++){
		printf("Level [%d] represents %3d dBm\n",i,TpcTable.t[i].TxPowerLevel);
	}
	return TRUE;
}

static const char *reset_help =
	"Usage:\n"
	"\n reset\n";

static void cmd_reset(int uart_fd, int argc, char **argv){
	int Length =0,iRet;
	UCHAR buf[3];

	if(argc > 1) {
		printf("\n%s\n",reset_help);
		return;
	}

	memset(&buf,0,sizeof(buf));
	buf[0] = 0;
	buf[1] = '\0';
	Length = 0;
	// OGF_HOST_CTL 0x03
	// OCF_RESET 0x0003
	iRet = writeHciCommand(uart_fd, 0x03, 0x0003,Length,buf);
	if(buf[5] != 0){
		printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
		return;
	}else{
		printf("\nHCI Reset Pass");
	}

	printf("\nReset Done\n");
}
static const char *rba_help =
	"Usage:\n"
	"\n rba\n";

static void cmd_rba(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc > 1){
		printf("\n%s\n",rba_help);
		return;
	}
	memset(&buf,0,MAX_EVENT_SIZE);
	// OGF_INFO_PARAM 0x04
	// OCF_READ_BD_ADDR 0x0009
	iRet = writeHciCommand(uart_fd, 0x04, 0x0009, 0, buf);
	printf("iRet: %d\n", iRet);
	if(buf[5] != 0){
		printf("\nread bdaddr command failed due to reason 0x%X\n",buf[6] );
		return;
	}
	printf("\nBD ADDRESS: \n");
	int i;
	for(i=iRet-1;i > 7;i--){
		printf("%02X:",buf[i]);
	}
	printf("%X \n\n",buf[7]);
}

static const char *dtx_help =
	"Usage:\n"
	"\n dtx\n";

static void cmd_dtx(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc > 1){
		printf("\n%s\n",dtx_help);
		return;
	}

        memset(&buf,0,MAX_EVENT_SIZE);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_DISABLE_TX, 0, buf);
	if(buf[6] != 0){
		printf("\nDisable TX command failed due to reason 0x%X\n",buf[6]);
		return;
	}
    else {
		printf("\nDisable TX command passed\n");
	}
}

static const char *ssm_help =
	"Usage:\n"
	"\n ssm [0|1]\n"
	"\nExample:\n"
	"\tssm 0\t(Sleep disabled)\n"
	"\tssm 1\t(Sleep enabled)\n";

static void cmd_ssm(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc != 2){
		printf("\n%s\n",ssm_help);
		return;
	}

	if(atoi(argv[1]) > 1){
		printf("\nInvalid sleep mode :%d\n",atoi(argv[1]));
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = atoi(argv[1]);;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE, 1, buf);
	if(buf[6] != 0){
		printf("\nSet sleep mode command failed due to reason 0x%X\n",buf[6]);
		return;
	}
        else {
             printf("\nSet sleep mode command passed\n");
        }
}

static const char *wba_help =
	"Usage:\n"
	"\n wba <bdaddr>\n"
	"\nExample:\n"
	"\n wba 00:03:ff:56:23:89\n";

static void cmd_wba(int uart_fd, int argc, char **argv){
	//printf("\nFeature not implemented\n");
	int iRet;
	bdaddr_t bdaddr;

	UCHAR buf[MAX_EVENT_SIZE];
	if(argc < 2){
		printf("\n%s\n",wba_help);
		return;
	}

	str2ba(argv[1],&bdaddr);
	if((strlen(argv[1]) < 17)||(strlen(argv[1]) > 17)){
		printf("\nInvalid BD address : %s\n",argv[1]);
		printf("\n%s\n",wba_help);
		return;
	}
	LoadPSHeader(buf,PS_WRITE,BD_ADDR_SIZE,BD_ADDR_PSTAG);
	int i,j=0;
	for(i= 0,j=3;i< BD_ADDR_SIZE;i++,j++){
		buf[j] = bdaddr.b[i];
        }
	iRet = hci_send_cmd(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,BD_ADDR_SIZE + 3, buf);

	memset(&buf,0,sizeof(buf));
	// OGF_HOST_CTL	 0x03
	// OCF_RESET 0x0003
	iRet = writeHciCommand(uart_fd, 0x03, 0x0003, 0, buf);

	memset(&buf,0,sizeof(buf));
	// OGF_INFO_PARAM 0x04
	// OCF_READ_BD_ADDR 0x0009
	iRet = writeHciCommand(uart_fd, 0x04, 0x0009, 0, buf);

	printf("\nBD address changed \n");
}

static const char *edutm_help =
	"Usage:\n"
	"\n edutm\n";

static void cmd_edutm(int uart_fd, int argc, char **argv){
	int Crc = 0;
	//UCHAR buf[MAX_EVENT_SIZE];
	UCHAR ZeroBuf[MEM_BLK_DATA_MAX*2] = {0};
	if(argc > 1){
		printf("\n%s\n",edutm_help);
		return;
	}
/*
	Patch_Count = 20;
	for(i=0; i < Patch_Count; i++){
		RamPatch[i].Len = MAX_BYTE_LENGTH;
		memset(&RamPatch[i].Data,0,MAX_BYTE_LENGTH);
	}
	printf("\nCMD DUT MODE\n");
*/
	if(!Dut(uart_fd)){
		return;
	}
	printf("\nDevice is in test mode ...\n");
	return;
}


static int ReadMemorySmallBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length ){
	int iRet;
	UCHAR *pData;
	UCHAR buf[MAX_EVENT_SIZE];
	pData = (UCHAR *) malloc(Length + 6);
	memset(pData,0,Length+6);
	pData[0]= 0x00;  //Memory Read Opcode
	pData[1]= (StartAddress & 0xFF);
	pData[2]= ((StartAddress >> 8) & 0xFF);
	pData[3]= ((StartAddress >> 16) & 0xFF);
	pData[4]= ((StartAddress >> 24) & 0xFF);
	pData[5]= Length;

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_MEMOP,Length+6,pData);
	if(pData[6]!= 0){
		printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);
		free(pData);
		return FALSE;
	}
	int plen =0;
	do{
		plen = read(uart_fd, buf,MAX_EVENT_SIZE);
		if (plen < 0) {
			free(pData);
			perror("Read failed");
			exit(EXIT_FAILURE);
		}
	}while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_MEMBLK);
	memcpy(pBufToWrite,(buf+HCI_EVENT_HEADER_SIZE+1),Length);
	free(pData);
	return TRUE;
}

static int ReadMemoryBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length ){

	int ModResult,i;

	if(Length > MEM_BLK_DATA_MAX){
		ModResult = Length % MEM_BLK_DATA_MAX;
		for(i=0;i < (Length - ModResult);i += MEM_BLK_DATA_MAX) {
			ReadMemorySmallBlock(uart_fd, (StartAddress + i),(pBufToWrite + i), MEM_BLK_DATA_MAX);
		}
		if(ModResult){
			ReadMemorySmallBlock(uart_fd, (StartAddress + i),(pBufToWrite + i), ModResult);
		}
	}
	else{

		ReadMemorySmallBlock(uart_fd, StartAddress, pBufToWrite, Length);
	}
	return TRUE;
}

static int WriteMemorySmallBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length ){
	int iRet;
	UCHAR *pData;

	printf("\nStart Address:%x Length:%x  %x\n",StartAddress,Length,MEM_BLK_DATA_MAX);
	/*if(Length <= MEM_BLK_DATA_MAX)
		return FALSE; */
	pData = (UCHAR *) malloc(Length + 6);
	memset(pData,0,Length+6);
	pData[0]= 0x01;  //Write Read Opcode
	pData[1]= (StartAddress & 0xFF);
	pData[2]= ((StartAddress >> 8) & 0xFF);
	pData[3]= ((StartAddress >> 16) & 0xFF);
	pData[4]= ((StartAddress >> 24) & 0xFF);
	pData[5]= Length;

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_MEMOP,Length+6,pData);
	if(pData[6]!= 0){
		printf("\nwrite memory command failed due to reason 0x%X\n",pData[6]);
		free(pData);
		return FALSE;
	}
	free(pData);
	return TRUE;
}


static int WriteMemoryBlock(int uart_fd, int StartAddress,UCHAR *pBufToWrite, int Length ){

	int ModResult,i;

	if(Length > MEM_BLK_DATA_MAX){
		ModResult = Length % MEM_BLK_DATA_MAX;
		for(i=0;i < (Length - ModResult);i += MEM_BLK_DATA_MAX) {
			WriteMemorySmallBlock(uart_fd, (StartAddress + i),(pBufToWrite + i), MEM_BLK_DATA_MAX);
		}
		if(ModResult){
			WriteMemorySmallBlock(uart_fd, (StartAddress + i),(pBufToWrite + i), ModResult);
		}
	}
	else{

		WriteMemorySmallBlock(uart_fd, StartAddress, pBufToWrite, Length);
	}
	return TRUE;
}


static int ReadHostInterest(int uart_fd,tBtHostInterest *pHostInt){
	UCHAR buf[MAX_EVENT_SIZE];
	int iRet;
	int HostInterestAddress;
	memset(&buf,0,MAX_EVENT_SIZE);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_HOST_INTEREST, 0, buf);
	if(buf[6] != 0){
		printf("\nhost interest command failed due to reason 0x%X\n",buf[6]);
		return FALSE;
	}
	HostInterestAddress = buf[iRet-1];
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-2]);
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-3]);
	HostInterestAddress = ((HostInterestAddress << 8)|buf[iRet-4]);
	ReadMemoryBlock(uart_fd, HostInterestAddress,(UCHAR*)pHostInt, sizeof(tBtHostInterest));

	if(pHostInt->MagicNumber != HI_MAGIC_NUMBER){
		if((pHostInt->MagicNumber != 0xFBAD)|| (pHostInt->Version != 0xDECA))
			return 0;
	}
	return TRUE;
}

static int contRxAtGivenChannel(int uart_fd, UCHAR *pString){
	int Address, Mask, Reg, RxFreq,iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	//1. Disable all scans and set intervals and scan windows eually
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0; //All scan disabled
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_SCAN_ENABLE 0x001A
	iRet = writeHciCommand(uart_fd, 0x03, 0x001A, 1, buf);
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	short int inq_scan = 0x1000;
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = (inq_scan&0xFF);
	buf[1] = ((inq_scan >> 8)& 0xFF);
	buf[2] = (inq_scan&0xFF);
	buf[3] = ((inq_scan >> 8)& 0xFF);
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_INQ_ACTIVITY 0x001E
	iRet = writeHciCommand(uart_fd, 0x03, 0x001E, 4, buf);
	if(buf[6] != 0){
		printf("\nWrite inquiry scan activity command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = (inq_scan&0xFF);
	buf[1] = ((inq_scan >> 8)& 0xFF);
	buf[2] = (inq_scan&0xFF);
	buf[3] = ((inq_scan >> 8)& 0xFF);
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_PAGE_ACTIVITY 0x001C
	iRet = writeHciCommand(uart_fd, 0x03, 0x001C, 4, buf);
	if(buf[6] != 0){
		printf("\nWrite page scan activity command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	//2. Disbable AGC
	Address = LC_JTAG_MODEM_REGS_ADDRESS + AGC_BYPASS_ADDRESS;
	Mask = AGC_BYPASS_ENABLE_MASK;
	Reg = AGC_BYPASS_ENABLE_SET(1);
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite to AGC bypass register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 3. Disable frequency hoping and set rx frequency

	RxFreq = (int)pString;
	Address = LC_DEV_PARAM_CTL_ADDRESS;
	Mask = LC_DEV_PARAM_CTL_FREQ_HOP_EN_MASK |
			LC_DEV_PARAM_CTL_RX_FREQ_MASK     |
			LC_DEV_PARAM_CTL_WHITEN_EN_MASK;
	Reg = LC_DEV_PARAM_CTL_RX_FREQ_SET(RxFreq);
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite to Rx Freq register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 4. Enable page scan only (Note: the old way puts device into inq scan mode only ???)
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 2; //Page scan enabled
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_SCAN_ENABLE 0x001A
	iRet = writeHciCommand(uart_fd, 0x03, 0x001A, 1, buf);
	if(buf[6] != 0){
		printf("\nPage scan enable command failed due to reason 0x%X\n",buf[6]);
		return 0;
	}
	// 5. Increase correlator
	Address = LC_JTAG_MODEM_REGS_ADDRESS + CORR_PARAM1_ADDRESS;
	Mask = CORR_PARAM1_TIM_THR_MASK;
	Reg = CORR_PARAM1_TIM_THR_SET(0x3f);
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = (Address & 0xFF);
	buf[1] = ((Address >>8) & 0xFF);
	buf[2] = ((Address>>16) & 0xFF);
	buf[3] = ((Address>>24) & 0xFF);
	buf[4] = 0x04;  //Memory width
	buf[5] = (Reg & 0xFF);
	buf[6] = ((Reg >> 8) & 0xFF);
	buf[7] = ((Reg >> 16) & 0xFF);
	buf[8] = ((Reg >> 24) & 0xFF);
	buf[9] = (Mask & 0xFF);
	buf[10] = ((Mask >>8) & 0xFF);
	buf[11] = ((Mask>>16) & 0xFF);
	buf[12] = ((Mask>>24) & 0xFF);

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_WRITE_MEMORY, 12, buf);
	if(buf[6] != 0){
		printf("\nWrite to Correlator register failed due to reason 0x%X\n",buf[6]);
		return 0;
	}

	return TRUE;
}
static const char *cwrx_help =
	"Usage:\n"
	"\n cwrx <Channel>\n";

static void cmd_cwrx(int uart_fd, int argc, char **argv){

	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR channel;
	BOOL Ok = TRUE;
	if(argc != 2){
		printf("\n%s\n",cwrx_help);
		return;
	}

	channel = atoi(argv[1]);
	if(channel > 78 || channel < 0){
		printf("\nPlease enter channel 0-78!\n");
		return;
	}

	// Disable sleep mode
	memset(&buf,0,sizeof(buf));
	buf[0] = 0;
	iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
	if(buf[6] != 0){
		printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
		Ok = 0;
	}
	printf (" Continuoux Rx at channel %d\n",channel);
	Ok = contRxAtGivenChannel(uart_fd, &channel);

         // All modes come here
	if (Ok) {
		printf (" Continuoux Rx at channel %d Done...\n",channel);
	}
	else {
		printf ("\nERROR ---> Could not enter continuous Rx mode\n");
	}
}

int OCFRXTestMode(int uart_fd, tBRM_Control_packet *MasterBlaster, UCHAR SkipRxSlot)
{
	int iRet;
	BOOL Ok = TRUE;
	UCHAR buf[MAX_EVENT_SIZE];

	memset(&buf,0,sizeof(buf));
	buf[0] = 0;
	iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
	if(buf[6] != 0) {
		printf ("\nERROR ---> Could not disable sleep mode\n");
		return -1;
	}
    Ok = Dut(uart_fd);
	if (!Ok) {
		printf("\nERROR ---> Could not enter DUT mode\n");
		return -1;
	}
	printf(".");

	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = eBRM_TestMode_Rx;
	buf[1] = MasterBlaster->testCtrl.Packet;
	buf[2] = MasterBlaster->testCtrl.DataLen & 0xFF;
	buf[3] = ((MasterBlaster->testCtrl.DataLen>>8) & 0xFF);
	buf[4] = MasterBlaster->testCtrl.HopMode;
	buf[5] = MasterBlaster->testCtrl.TxFreq;
	buf[6] = MasterBlaster->testCtrl.Power;
	buf[7] = MasterBlaster->testCtrl.RxFreq;
	buf[8] = MasterBlaster->bdaddr[0];
	buf[9] = MasterBlaster->bdaddr[1];
	buf[10] = MasterBlaster->bdaddr[2];
	buf[11] = MasterBlaster->bdaddr[3];
	buf[12] = MasterBlaster->bdaddr[4];
	buf[13] = MasterBlaster->bdaddr[5];
	buf[14] = SkipRxSlot;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_RX_TESTER, 15, buf);
	if (buf[6] != 0) {
		printf("\nRx Tester command failed due to reason 0x%X\n",buf[6]);
		printf("\nERROR --> Could not enable master blaster mode\n");
		Ok = 0;
		return MB_NO_TEST;
	}
	printf(" rx test is in progress. Press 's' to stop the test\n");
	return MB_RX_TEST;
}

int OCFTXTestMode(int uart_fd, tBRM_Control_packet *MasterBlaster, UCHAR SkipRxSlot)
{
	int iRet;
	BOOL Ok = TRUE;
	UCHAR buf[MAX_EVENT_SIZE];

        memset(&buf,0,sizeof(buf));
        buf[0] = 0;
        iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
        if(buf[6] != 0) {
		printf ("\nERROR ---> Could not disable sleep mode\n");
		return -1;
        }

        printf (".");
        Ok = Dut(uart_fd);
        if (!Ok) {
		printf("\nERROR ---> Could not enter DUT mode\n");
		return -1;
        }

        printf(".");
        memset(&buf,0,MAX_EVENT_SIZE);
        buf[0] = MasterBlaster->testCtrl.Mode;
        buf[1] = MasterBlaster->testCtrl.Packet;
        buf[2] = MasterBlaster->testCtrl.DataLen & 0xFF;
        buf[3] = ((MasterBlaster->testCtrl.DataLen>>8) & 0xFF);
        buf[4] = MasterBlaster->testCtrl.HopMode;
        buf[5] = MasterBlaster->testCtrl.TxFreq;
        buf[6] = MasterBlaster->testCtrl.Power;
        buf[7] = MasterBlaster->testCtrl.RxFreq;
        buf[8] = MasterBlaster->bdaddr[0];
        buf[9] = MasterBlaster->bdaddr[1];
        buf[10] = MasterBlaster->bdaddr[2];
        buf[11] = MasterBlaster->bdaddr[3];
        buf[12] = MasterBlaster->bdaddr[4];
        buf[13] = MasterBlaster->bdaddr[5];
        buf[14] = SkipRxSlot;
        iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_TX_TESTER, 15, buf);
        if (buf[6] != 0) {
			printf("\nTx Tester command failed due to reason 0x%X\n",buf[6]);
			printf("\nERROR --> Could not enable master blaster mode\n");
			return MB_NO_TEST;
        }
        printf(" tx test is in progress. Press 's' to stop the test\n");
        return MB_TX_TEST;
}

int OCFContTXTestMode(int uart_fd, tBRM_Control_packet *MasterBlaster)
{
	BOOL Ok = TRUE;
	UCHAR buf[MAX_EVENT_SIZE];
	int iRet, address, width, value, mask;

        memset(&buf,0,sizeof(buf));
        buf[0] = 0;
        iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
        if(buf[6] != 0) {
                printf ("\nERROR ---> Could not disable sleep mode\n");
                return -1;
        }

        Ok = Dut(uart_fd);
        if (!Ok) {
			printf("\nERROR ---> Could not enter DUT mode\n");
			return -1;
		}

        /* Enable master blaster mode */
        printf(".");
/*
        if (CW_Single_Tone == MasterBlaster.ContTxType)
			setContTxType = Cont_Tx_Raw_1MHz;
        else
                setContTxType = MasterBlaster.ContTxType;
*/
        memset(&buf, 0, MAX_EVENT_SIZE);
        buf[0] = MasterBlaster->testCtrl.Mode ;
        buf[1] = MasterBlaster->testCtrl.Packet;
        buf[2] = MasterBlaster->testCtrl.DataLen & 0xFF;
        buf[3] = ((MasterBlaster->testCtrl.DataLen>>8) & 0xFF);
        buf[4] = MasterBlaster->ContTxType;
        buf[5] = MasterBlaster->testCtrl.TxFreq;
        buf[6] = MasterBlaster->testCtrl.Power;
        buf[7] = MasterBlaster->testCtrl.RxFreq;
        buf[8] = MasterBlaster->bdaddr[0];
        buf[9] = MasterBlaster->bdaddr[1];
        buf[10] = MasterBlaster->bdaddr[2];
        buf[11] = MasterBlaster->bdaddr[3];
        buf[12] = MasterBlaster->bdaddr[4];
        buf[13] = MasterBlaster->bdaddr[5];
        iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_CONT_TX_TESTER, 14, buf);
        if(buf[6] != 0){
		printf("\nContinious Tx Tester command failed due to reason 0x%X\n",buf[6]);
		return MB_NO_TEST;
	}

        memset(&buf,0,MAX_EVENT_SIZE);
        address = 0x00022914;
        value = 0x00200000;
        mask = 0x00200000;
        width = 4;
        buf[0] = (address & 0xFF);
        buf[1] = ((address >>8) & 0xFF);
        buf[2] = ((address>>16) & 0xFF);
        buf[3] = ((address>>24) & 0xFF);
        buf[4] = width;  //Memory width
        buf[5] = (value & 0xFF);
        buf[6] = ((value >> 8) & 0xFF);
        buf[7] = ((value >> 16) & 0xFF);
        buf[8] = ((value >> 24) & 0xFF);
        buf[9] = (mask & 0xFF);
        buf[10] = ((mask >>8) & 0xFF);
        buf[11] = ((mask>>16) & 0xFF);
        buf[12] = ((mask>>24) & 0xFF);
        iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_WRITE_MEMORY, 13, buf);
        if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return MB_NO_TEST;
	}
        return MB_CONT_TX_TEST;
}

int OCFContRXTestMode(int uart_fd, tBRM_Control_packet *MasterBlaster)
{
        BOOL Ok = TRUE;
        UCHAR buf[MAX_EVENT_SIZE];
        int iRet;

        memset(&buf,0,sizeof(buf));
        buf[0] = 0;
        iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
        if(buf[6] != 0) {
                printf ("\nERROR ---> Could not disable sleep mode\n");
                return -1;
        }

        UCHAR RxFreq = MasterBlaster->testCtrl.RxFreq;
        Ok = contRxAtGivenChannel(uart_fd, &RxFreq);
        return MB_CONT_RX_TEST;
}

static const char *mb_help =
	"Usage:\n"
	"\n mb\n";

static void cmd_mb(int uart_fd, int argc, char **argv){

	int FieldNum,iRet,iDataSize, fdmax, k, l, i, j;
	bdaddr_t bdaddr;
	tBRM_Control_packet MasterBlaster;
	UCHAR	SkipRxSlot;
	UCHAR buf[MAX_EVENT_SIZE];
	char FieldAlias;
	BOOL TestEnabled = 0,Ok = TRUE;
	UINT8 setContTxType;
	tBtHostInterest	HostInt;
	fd_set master, read_fds;
        uint32_t m_BerTotalBits, m_BerGoodBits;
		uint8_t m_pattern[16];
		uint16_t m_pPatternlength;
		struct timeval timeout;

	memset(&buf,0,MAX_EVENT_SIZE);
	// OGF_INFO_PARAM 0x04
	// OCF_READ_BD_ADDR 0x0009
	iRet = writeHciCommand(uart_fd, 0x04, 0x0009, 0, buf);
	if (buf[6] != 0) {
		printf("\nread bdaddr command failed due to reason 0x%X\n",buf[6]);
		return;
	}
	char bda[18];
	for (i=iRet-1,j=0;i>7;i--,j+=3) {
		snprintf(&bda[j],sizeof(bda[j]),"%X",((buf[i]>>4)&0xFF));
		snprintf(&bda[j+1],sizeof(bda[j+1]),"%X",(buf[i]&0x0F));
		snprintf(&bda[j+2],sizeof(bda[j+2]),":");
	}
	snprintf(&bda[15],sizeof(bda[15]),"%X",((buf[7]>>4)&0xFF));
	snprintf(&bda[16],sizeof(bda[16]),"%X",(buf[7]&0x0F));
	bda[18] ='\0';
	str2ba(bda,&bdaddr);

	InitMasterBlaster(&MasterBlaster, &bdaddr, &SkipRxSlot);
#ifndef DUMP_DEBUG
	Ok = ReadHostInterest(uart_fd,&HostInt);
	if(Ok) {
		if (HostInt.TpcTableAddr && (HostInt.Version >= 0x0100)) {
			Ok = ReadMemoryBlock(uart_fd, HostInt.TpcTableAddr, (UCHAR *)&TpcTable, sizeof(TpcTable));
			MasterBlaster.testCtrl.Power = TpcTable.NumOfEntries - 1;
		}
	}
	if(!Ok) {
	printf ("\nCould not load TPC table.\n");
	sleep (2);
	Ok = TRUE;
	}
#endif

        if (!strncmp(argv[1],"cmd",3)) {
		MasterBlaster.testCtrl.Power = atoi(argv[2]);
		MasterBlaster.testCtrl.RxFreq = atoi(argv[3]);
		MasterBlaster.testCtrl.TxFreq = atoi(argv[3]);
		for (i = 0; i < sizeof(PacketTypeOption)/sizeof(tMasterBlasterOption); ++i)
		{
			printf("%s %s\n",argv[4],PacketTypeOption[i].Name);
			if (!strcmp(argv[4],PacketTypeOption[i].Name)) {
				MasterBlaster.testCtrl.Packet = PacketTypeOption[i].Value;
				MasterBlaster.testCtrl.DataLen = MaxDataLenOption[i];
				printf("%s %s\n",argv[4],PacketTypeOption[i].Name);
				break;
			}
		}
		if (!strncmp(argv[5],"Tx",2)) {
			OCFTXTestMode(uart_fd, &MasterBlaster, SkipRxSlot);
			printf("Tx\n");
		}
		else if (!strncmp(argv[5],"Rx",2)) {
			OCFRXTestMode(uart_fd, &MasterBlaster, SkipRxSlot);
			printf("Rx\n");
		}
		getchar();
                memset(&buf,0,sizeof(buf));
				// OGF_HOST_CTL	 0x03
				// OCF_RESET 0x0003
                iRet = writeHciCommand(uart_fd, 0x03, 0x0003,0,buf);
                if (buf[6] != 0) {
                        printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
                }
		return;
        }
	else if (argc > 1) {
		printf("\n%s\n", mb_help);
		return;
	}

	PrintMasterBlasterMenu (&MasterBlaster);
    m_BerGoodBits = 0;
    m_BerTotalBits = 0;
    m_pattern[0] = 0x0f;
    m_pPatternlength = 1;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_SET(0, &master);
	FD_SET(uart_fd, &master);
	fdmax = uart_fd;

   while (1) {
        read_fds = master;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        iRet = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
        if (iRet == -1) {
            perror("cmd_mb select() error!");
            goto exits_mb;
        }
        if (iRet == 0) continue;

        for(i = 0; i <= fdmax; i++) {
	    if(FD_ISSET(i, &read_fds)) {
			if (i==0) {	// input
				scanf("%s",buf);
				FieldAlias = (char)buf[0];
				FieldNum = CheckField(MasterBlaster, &FieldAlias);
				if (FieldNum == INVALID_MASTERBLASTER_FIELD) {
					printf ("\nERROR ---> Invalid command. Try again.\n");
					printf ("mb>");
					continue;
				}

				if (!strncmp(&FieldAlias, MasterBlasterMenu[EXX].Alias, 1)) {
				printf("\nExit the Master Blaster Mode without reset\n");
				goto exits_mb;
				}

	            // if the test is in rx and the key is neither 'd' nor 'g', then stop the test, renew the option, and procced
		    // if the test is in tx and the key is not 'e', then stop the test, renew the option, and procced
		    // if the test is in (LE) continuous rx/tx and the key is not 'j' , then stop the test, renew the option, and procced
		    if (((TestEnabled == MB_RX_TEST) && strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && strncmp(&FieldAlias, MasterBlasterMenu[GB].Alias, 1))
				|| ((TestEnabled == MB_TX_TEST) && strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1))
				|| ((TestEnabled == MB_CONT_RX_TEST || TestEnabled == MB_CONT_TX_TEST
				|| TestEnabled == MB_LE_RX_TEST || TestEnabled == MB_LE_TX_TEST) &&
				(strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)))) {
				printf (" ... Please wait ...");
				if (MasterBlaster.ContTxMode) {
					memset(&buf,0,MAX_EVENT_SIZE);
					buf[0] = 255;
					iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_CONT_TX_TESTER, 14, buf);
				if(buf[6] != 0) {
					printf("\nContinious Tx Tester command failed due to reason 0x%X\n",buf[6]);
					Ok = 0;
				} else
					Ok = TRUE;
				}

			memset(&buf,0,sizeof(buf));
			// OGF_HOST_CTL	 0x03
			// OCF_RESET 0x0003
			iRet = writeHciCommand(uart_fd,0x03,0x0003,0,buf);
			if (buf[6] != 0) {
				printf("\nError: HCI RESET failed due to reason 0x%X\n",buf[6]);
				Ok = FALSE;
			} else
				Ok = TRUE;
				if (!Ok) {
		            printf ("\nERROR ---> Could not stop test mode\n");
				} else if (!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1)
				|| !strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1)
				|| ((TestEnabled != MB_NO_TEST) &&
				(!strncmp(&FieldAlias, MasterBlasterMenu[CR].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[CT].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[LR].Alias, 1) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[LT].Alias, 1))) ||
				 !strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) {
				TestEnabled = MB_NO_TEST;
			}
				sleep(1);
		    }
		    if (!strncmp(&FieldAlias,MasterBlasterMenu[EX].Alias,1)) {			// Exit
		        TestEnabled = MB_NO_TEST;
		        printf ("\n Exit ..\n");
				goto exits_mb;
				} else if (!strncmp(&FieldAlias,MasterBlasterMenu[ST].Alias,1)) {		// Stop Test
			TestEnabled = MB_NO_TEST;
		        PrintMasterBlasterMenu (&MasterBlaster);
			continue;
		    } else if (!strncmp(&FieldAlias,MasterBlasterMenu[GB].Alias,1)) {		// get BER
		        printf("\n\tGoodBits %d, total is %d\n", m_BerGoodBits, m_BerTotalBits);
		        printf("mb>\n");
		        continue;
		    } else if (!strncmp(&FieldAlias,MasterBlasterMenu[PO].Alias,1)) {		// set Power
			MasterBlasterMenu[FieldNum].pFunc (&MasterBlaster, &FieldAlias);
			} else if (!MasterBlasterMenu[FieldNum].pFunc (&MasterBlaster, MasterBlasterMenu[FieldNum].Options)) {
			printf ("\nERROR ---> Invalid option. Try again.\n");
		        printf ("mb>");
		        continue;
			}
		    PrintMasterBlasterMenu(&MasterBlaster);

                // Enable RX test mode
                    if ((!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && (TestEnabled == MB_NO_TEST))
						|| (strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1) && (TestEnabled == MB_RX_TEST))) {

			iRet = OCFRXTestMode(uart_fd, &MasterBlaster, SkipRxSlot);
			if (iRet != -1)
				TestEnabled = iRet;
                        printf("mb>");
                        continue;
                    } else if ((!strncmp(&FieldAlias, MasterBlasterMenu[RX].Alias, 1)) && (TestEnabled == MB_RX_TEST)) {
                        printf(" rx test is in progress. Press 's' to stop the test\n");
                        printf("mb>");
                        continue;
                    }

			// Enable TX test mode
		    if ((!strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1) && (TestEnabled == MB_NO_TEST))
				|| (strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1) && (TestEnabled == MB_TX_TEST))) {
		        // Disable sleep mode
				printf (".");
				Ok = TRUE;
				iRet = OCFTXTestMode(uart_fd, &MasterBlaster, SkipRxSlot);
				if (iRet != -1)
					TestEnabled = iRet;
				printf("mb>");
				continue;
		    } else if ((!strncmp(&FieldAlias, MasterBlasterMenu[TX].Alias, 1)) && TestEnabled == MB_TX_TEST) {
			printf(" tx test is in progress. Press 's' to stop the test\n");
			printf("mb>");
			continue;
		    }

		/* Enable (LE) continuous tx/rx test modes */
		    if (((!strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) && (TestEnabled == MB_NO_TEST))
			|| (strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1) && (TestEnabled == MB_CONT_RX_TEST
			|| TestEnabled == MB_CONT_TX_TEST || TestEnabled == MB_LE_RX_TEST || TestEnabled == MB_LE_TX_TEST))) {

			if (MasterBlaster.ContTxMode == ENABLE) {
				printf(".");
                                iRet = OCFContTXTestMode(uart_fd, &MasterBlaster);
                                if (iRet != -1) {
                                        TestEnabled = iRet;
                                }
                                printf("mb>");
                                continue;
			} else if (MasterBlaster.ContRxMode == ENABLE) {
				printf(".");
				iRet = OCFContRXTestMode(uart_fd, &MasterBlaster);
				if (iRet != -1) {
					TestEnabled = iRet;
				}
				printf("mb>");
				continue;
			}

		        // Disable sleep mode
			printf (".");
			Ok = TRUE;
			memset(&buf,0,sizeof(buf));
			buf[0] = 0;
			iRet = writeHciCommand(uart_fd,HCI_VENDOR_CMD_OGF,OCF_SLEEP_MODE,1,buf);
			if (buf[6] != 0) {
				printf("\nError: Sleep mode failed due to reason 0x%X\n",buf[6]);
				Ok = 0;
			}

			if (!Ok) {
				printf ("\nERROR ---> Could not disable sleep mode\n");
				printf ("mb>");
				continue;
			}

			/*  LE Rx Mode */
			if (MasterBlaster.LERxMode == ENABLE) {
				Ok = SU_LERxTest(uart_fd, MasterBlaster.testCtrl.RxFreq);
				TestEnabled = MB_LE_RX_TEST;
			} else if (MasterBlaster.LETxMode == ENABLE) {
				Ok = SU_LETxTest(uart_fd, MasterBlaster.testCtrl.TxFreq, MasterBlaster.testCtrl.DataLen,
						MasterBlaster.LETxParms.PktPayload);
				TestEnabled = MB_LE_TX_TEST;
			}
			if (Ok) {
				printf("Test is in progress. Press 's' to stop the test\n");
			} else {
				printf("\nERROR ---> Could not enable master blaster mode\n");
				TestEnabled = MB_NO_TEST;
			}
			printf("mb>");
			continue;
			} else if ((!strncmp(&FieldAlias, MasterBlasterMenu[EN].Alias, 1)) && TestEnabled) {
			printf (" Test mode is in progress. Press 's' to stop the test\n");
			printf ("mb>");
			continue;
			}
	        }
			else if (i == uart_fd) {
                    iRet = read(i, buf, sizeof(buf));
                    iDataSize = iRet - 5;
//		    printf("first:%x,nbyte:%d, packet:%d, pattern:%x\n",buf[0], iRet, (uint16_t)(buf[3] | (buf[4] << 8)), buf[5]);
                    if (buf[0] == 0x2) {        // ACL data
                            m_BerTotalBits = m_BerTotalBits + iDataSize * 8;
                            for(j=0,l=0;j<iDataSize;j++,l++) {
                                if (l == m_pPatternlength) l = 0;
                                for(k=0;k<8;k++){
                                    if((m_pattern[l]&(1<<k)) == (buf[8+j]&(1<<k)))
                                    m_BerGoodBits++;
                                }
                            }
                    }
			}
	    }
	}
   }
exits_mb:

  return;
}
/*
static void cmd_gid(int uart_fd, int argc, char **argv){
	printf("\nFeature not implemented\n");
}
*/
static const char *wsm_help =
	"Usage:\n"
	"\n wsm [0|1|2|3]\n"
	"\nExample:\n"
	"\twsm 0\t(Scan disabled)\n"
	"\twsm 1\t(Inquiry scan enabled)\n"
	"\twsm 2\t(Page scan enabled)\n"
	"\twsm 3\t(Inquiry and Page scan enabled)\n";

static void cmd_wsm(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	if(argc < 2){
		printf("\n%s\n",wsm_help);
		return;
	}
	if(atoi(argv[1]) > 3){
		printf("\nInvalid scan mode :%d\n",atoi(argv[1]));
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = atoi(argv[1]);
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_SCAN_ENABLE 0x001A
	iRet = writeHciCommand(uart_fd, 0x03, 0x001A, 1, buf);
	if(buf[6] != 0){
		printf("\nWrite scan mode command failed due to reason 0x%X\n",buf[6]);
		return;
	}

	printf("\nScan Mode set to :%d\n",atoi(argv[1]));
}

static void dumpHex(UCHAR *buf, int length, int col)
{
	int i;
	for (i = 0; i < length; i++) {
		printf("0x%02x ", buf[i]);
		if (((i+1) % col) == 0 && i != 0)
			printf("\n");
	}
	if (((i+1) % col) != 0) printf("\n");
}

static void ReverseHexString(char *pStr)
{
	int i, j;
	char temp;
	int len = strlen(pStr);

	for (i = 0; pStr[i] == ' ' || pStr[i] == '\t'; i++);

	if (pStr[i] == '0' && pStr[i+1] == 'x')
		i += 2;

	for (j = len - 1; i < j - 2; i += 2, j -= 2) {
		temp = pStr[i];
		pStr[i] = pStr[j - 1];
		pStr[j - 1] = temp;
		temp = pStr[i + 1];
		pStr[i + 1] = pStr[j];
		pStr[j] = temp;
	}
}

static void GetByteSeq(UCHAR *pDst, UCHAR *pSrc, int Size)
{
	UCHAR LowNibble, Nibble = 0;
	UCHAR *pLastHex;
	UCHAR *pStr = pSrc;

	while (*pStr == ' ' || *pStr == '\t') pStr++;

	if ((pStr[0] == '0') && (pStr[1] == 'x'))
		pStr += 2;

	pLastHex = pStr - 1;
	while (IS_HEX(*(pLastHex + 1)))
		pLastHex++;

	LowNibble = 0;

	while (Size > 0) {
		if (pStr <= pLastHex) {
			Nibble = CONV_HEX_DIGIT_TO_VALUE(*pStr);
			pStr++;
		} else {
			Nibble = 0;
		}

		if (LowNibble) {
			*pDst |= (UCHAR)(Nibble & 0x0F);
			LowNibble = 0;
			pDst++;
			Size--;
		} else {
			*pDst = (UCHAR)((Nibble << 4) & 0xF0);
			LowNibble = 1;
		}
	}
}

unsigned int GetUInt(char **ppLine, unsigned int DefaultValue)
{
  char *pStr = *ppLine;
  unsigned int Value = 0;

  // Is it a hex value?
  if ((*pStr == '0') && (*(pStr+1) == 'x'))
  {
    // We have a hex value

    pStr += 2;

    while (IS_HEX(*pStr))
    {
      Value = CONV_HEX_DIGIT_TO_VALUE(*pStr) + (Value*16);
      pStr++;
    }

  }
  else if (IS_DIGIT(*pStr))
  {
    // We have a decimal value
    while (IS_DIGIT(*pStr))
    {
      Value = CONV_DEC_DIGIT_TO_VALUE(*pStr) + (Value*10);
      pStr++;
    }
  }
  else
  {
    // We don't have a value at all - return default value
    return DefaultValue;
  }

  // Update the BtString ptr
  *ppLine = pStr;
  return Value;
}

static const char *mbr_help =
	"Usage:\n"
	"\n mbr <address> <length> \n"
	"\n Example \n"
	"\n mbr 0x00004FFC 10 \n"
	"\n mbr 0x00004FFC 0x10 \n";

static void cmd_mbr(int uart_fd, int argc, char **argv){

	UCHAR buf[MAX_EVENT_SIZE*20];

	if(argc != 3){
		printf("\n%s\n",mbr_help);
		return;
	}

        int length = GetUInt(&(argv[2]),0);
        int address  = GetUInt(&(argv[1]),0);

	if ((address == 0) || (length==0)){
		return;
	}
		memset(&buf,0,MAX_EVENT_SIZE*20);
		if(!MemBlkRead(uart_fd,address,buf, length)) {
			printf("\nmemory bulk read command failed\n");
			return;
		}
	printf("\ndata: \n");
	int i;
	for(i=0;i < length;i+=4){
        printf("%08X: ",address+i);
		printf("%08X",*((int*)(buf+i)));
        printf("\n");
	}
		printf("\n");
}

static const char *psr_help =
	"Usage:\n"
	"\n psr \n";

static void cmd_psr(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	if(argc > 1){
		printf("\n%s\n",psr_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	LoadPSHeader(buf,PS_RESET,0,0);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER+2, buf);
        if(buf[7] != 0){ /* Check for status */
		printf("\n PS Reset failed\n");
		return;
	}
	printf("PS reset done\n");
}

static const char *rpst_help =
	"Usage:\n"
	"\n rpst <tag id> <tag length> \n"
	"\n Example:\n"
	"\n rpst 1 6 \n";

static void cmd_rpst(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int tag_id,tag_len,i,j;
	if(argc != 3){
		printf("\n%s\n",rpst_help);
		return;
	}
	memset(&buf,0,MAX_EVENT_SIZE);
	tag_id = GetUInt(&(argv[1]),0);
	tag_len = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_READ,tag_len,tag_id);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\n read PS tag failed due to reason 0x%X\n",buf[6]);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	iRet = read(uart_fd,&buf,MAX_EVENT_SIZE);
	if(iRet < 0){
		printf("\n read PS tag failed\n");
		return;
	}
	printf("\nTag ID :%X\nTag Length:%X\nTag Data:\n",tag_id,tag_len);
	for(i=4,j=1;i<iRet;i++,j++){
		printf("%02X ",buf[i]);
		if(j%16 == 0)
			printf("\n");
	}
	printf("\n\n");
}
static const char *wpst_help =
	"Usage:\n"
	"\n wpst <tag id> <tag length> <tag data>\n"
	"\n Example:\n"
	"\n wpst 1 6 00 03 F4 55 AB 77 \n";

static void cmd_wpst(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int tag_id,tag_len,i;
	if(argc < 4){
		printf("\n%s\n",wpst_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	tag_id = GetUInt(&(argv[1]),0);
	tag_len = GetUInt(&(argv[2]),0);
	if(argc < tag_len+3){
		printf("\n Tag Data is less than Tag Length\n");
		return;
	}
	LoadPSHeader(buf,PS_WRITE,tag_len,tag_id);
	for(i=0;i<tag_len;i++){
		buf[i+4] = strtol(argv[i+3], NULL, 16);
	}
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER + tag_len, buf);
	if(buf[6] != 0){
		printf("\n Write PS tag failed due to reason 0x%X\n",buf[6]);
		return;
	}

}

static const char *setam_help =
	"Usage:\n"
	"\nsetam <storage medium> <access mode>\n"
	"\nstorage medium: 0-RAM  1-EEPROM\n"
	"\naccess mode: 0-Read-only 1-Write-only 2-Read-Write 3- Disabled\n"
	"\nExample:\n"
	"\nsetam 0 3\n";
static void cmd_setam(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int medium,mode;
	if(argc !=3){
		printf("\n%s\n",setam_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	medium = GetUInt(&(argv[1]),0);
	mode = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_SET_ACCESS_MODE,mode,medium);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\nSet Access mode failed due to reason 0x%X\n",buf[6]);

		return;
	}

	printf("\nAccess mode changed successfully!\n");
}

static const char *setap_help =
	"Usage:\n"
	"\nsetap <storage medium> <priority>\n"
	"\nstorage medium: 0-RAM  1-EEPROM\n"
	"\npriority: #Highest number corresponds to highest priority\n"
	"\nExample:\n"
	"\nsetap 0 1\n";

static void cmd_setap(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int medium,priority;
	if(argc !=3){
		printf("\n%s\n",setap_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	medium = GetUInt(&(argv[1]),0);
	priority = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_SET_ACCESS_MODE,priority,medium);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\nSet Access priority failed due to reason 0x%X\n",buf[6]);
		return;
	}

	printf("\nPriority changed successfully!\n");
}

static const char *rpsraw_help =
	"Usage:\n"
	"\n rpsraw <offset> <length> \n"
	"\n Example:\n"
	"\n rpsraw 0x012c 10\n";
static void cmd_rpsraw(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int offset,len,i,j;
	if(argc != 3){
		printf("\n%s\n",rpsraw_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	offset = GetUInt(&(argv[1]),0);
	len = GetUInt(&(argv[2]),0);
	LoadPSHeader(buf,PS_READ_RAW,len,offset);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER, buf);
	if(buf[6] != 0){
		printf("\n read PS raw failed due to reason 0x%X\n",buf[6]);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	iRet = read(uart_fd,&buf,MAX_EVENT_SIZE);
	if(iRet < 0){
		printf("\n read PS raw failed\n");
		return;
	}
	printf("\nOffset :%X\nLength:%X\nData:\n",offset,len);

	for(i=4,j=1;i<iRet;i++,j++){
		printf("%02X ",buf[i]);
		if(j%16 == 0)
			printf("\n");
	}
	printf("\n\n");

}
static const char *wpsraw_help =
	"Usage:\n"
	"\n wpsraw <offset> <length> <data>\n"
	"\n Example:\n"
	"\n wpsraw 0x012C 6 00 03 F4 55 AB 77 \n";

static void cmd_wpsraw(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int offset,len,i;
	if(argc < 4){
		printf("\n%s\n",wpsraw_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	offset = GetUInt(&(argv[1]),0);
	len = GetUInt(&(argv[2]),0);
	if(argc < len+3){
		printf("\nData is less than Length\n");
		return;
	}
	LoadPSHeader(buf,PS_WRITE_RAW,len,offset);
	for(i=0;i<len;i++){
		buf[i+4] = strtol(argv[i+3], NULL, 16);
	}
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS,PS_COMMAND_HEADER + len, buf);
	if(buf[6] != 0){
		printf("\n Write PS tag failed due to reason 0x%X\n",buf[6]);
		return;
	}
}

static const char *peek_help =
	"\nUsage:"
	"\npeek <address> <width>\n"
	"\nExample:\n"
	"\npeek 0x00004FFC 5\n";
static void cmd_peek(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int address,width,value;
	if(argc < 2){
		printf("\n%s\n",peek_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	address = GetUInt(&(argv[1]),0);
	if(argc == 3)
		width = GetUInt(&(argv[2]),0x4);
	else
		width = 4;

	buf[0] = (address & 0xFF);
	buf[1] = ((address >>8) & 0xFF);
	buf[2] = ((address>>16) & 0xFF);
	buf[3] = ((address>>24) & 0xFF);
	buf[4] = (UCHAR)width;  //Memory width
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_READ_MEMORY, 5, buf);
	if(buf[6] != 0){
		printf("\nRead Memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}
	value = buf[10];
	value = ((value << 8) | buf[9]);
	value = ((value << 8) | buf[8]);
	value = ((value << 8) | buf[7]);
	printf("\n0x%X : 0x%X \n",address,value);
	//printf("\n0x%X : 0x%02X%02X%02X%02X \n",address,buf[7],buf[8],buf[9],buf[10]);

}

static const char *cwtx_help =
	"\nUsage:"
	"\ncwtx <channel number>\n"
	"\nExample:\n"
        "\ncwtx 40"
	"\n\n";

static void cmd_cwtx(int uart_fd, int argc, char **argv){
	int iRet, Length = 0;
	UCHAR buf[MAX_EVENT_SIZE];
	int channel;
	if(argc != 2){
		printf("\n%s\n",cwtx_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	channel = atoi(argv[1]);
	if(channel > 78 || channel < 0){
		printf("\nPlease enter channel 0-78!\n");
		return;
	}
	memset(&buf,0,MAX_EVENT_SIZE);
	// OGF_HOST_CTL	 0x03
	// OCF_RESET 0x0003
	iRet = writeHciCommand(uart_fd, 0x03,0x0003,Length,buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0x80;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0xFF;
	buf[6] = 0x08;
	buf[7] = 0xC0;
	buf[8] = 0x00;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}
	/* hcitool cmd 0x3F 0x06 0x34 0x20 0x02 0x00 0x04 0x88 0xA0 0x00 0x02 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0x34;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x88;
	buf[6] = 0xA0;
	buf[7] = 0x00;
	buf[8] = 0x02;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}

	/* hcitool cmd 0x3F 0x06 0x28 0x20 0x02 0x00 0x04 0x00 0x90 0x05 0x20 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0x28;
	buf[1] = 0x20;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x00;
	buf[6] = 0x90;
	buf[7] = 0x05;
	buf[8] = 0x20;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}

	/* hcitool cmd 0x3F 0x06 0x7C 0x08 0x02 0x00 0x04 0x01 0x00 0x00 0x4B 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0x7C;
	buf[1] = 0x08;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = 0x01;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x4B;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}

	/* hcitool cmd 0x3F 0x06 0x00 0x08 0x02 0x00 0x04 $number 0x00 0x00 0x00 0xFF 0xFF 0xFF 0xFF */
	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = 0x00;
	buf[1] = 0x08;
	buf[2] = 0x02;
	buf[3] = 0x00;
	buf[4] = 0x04;
	buf[5] = (UCHAR)channel; /* Num */
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0xFF;
	buf[10] = 0xFF;
	buf[11] = 0xFF;
	buf[12] = 0xFF;
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF,OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}
	printf("\nEntering continuous wave Tx on channel %d\n",channel);

}


static const char *poke_help =
	"\nUsage:"
	"\npoke <address> <value> <mask> <width>\n"
	"\nExample:\n"
        "\npoke 0x580000 0x22005FF 0xFFFFFFFF 4"
	"\n\n";

static void cmd_poke(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	int address,width,value,mask;
	if(argc < 2){
		printf("\n%s\n",poke_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	address = GetUInt(&(argv[1]),0);
	value = GetUInt(&(argv[2]),0);
	printf("\nARGC :%d\n",argc);
	if(argc < 4)
		mask = 0xffffffff;
	else
		mask = GetUInt(&(argv[3]),0xFFFFFFFF);
	if(argc < 5)
		width = 4;
	else
		width = GetUInt(&(argv[4]),0x4);
	buf[0] = (address & 0xFF);
	buf[1] = ((address >>8) & 0xFF);
	buf[2] = ((address>>16) & 0xFF);
	buf[3] = ((address>>24) & 0xFF);
	buf[4] = width;  //Memory width
	buf[5] = (value & 0xFF);
	buf[6] = ((value >> 8) & 0xFF);
	buf[7] = ((value >> 16) & 0xFF);
	buf[8] = ((value >> 24) & 0xFF);
	buf[9] = (mask & 0xFF);
	buf[10] = ((mask >>8) & 0xFF);
	buf[11] = ((mask>>16) & 0xFF);
	buf[12] = ((mask>>24) & 0xFF);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_WRITE_MEMORY, 13, buf);
	if(buf[6] != 0){
		printf("\nWrite memory address failed due to reason 0x%X\n",buf[6]);
		return;
	}
	printf("\nPoke successful!\n");
}



static const char *dump_help =
	"\nUsage:"
	"\ndump audio - Display Audio statistics\n"
	"\ndump dma- Display DMA statistics\n"
	"\ndump dma r - Display and Reset DMA statistics\n"
	"\ndump tpc - Dump TPC tables\n"
	"\nExample:\n"
        "\ndump audio"
        "\ndump dma"
        "\ndump dma r"
        "\ndump tpc"
	"\n";


static void cmd_dump(int uart_fd, int argc, char **argv){

	if(argc < 2){
		printf("\n%s\n",dump_help);
		return;
	}

	if(!strncmp(argv[1],"audio",5)){
		ReadAudioStats(uart_fd);
	}
	else if(!strncmp(argv[1],"dma",3)){
		ReadGlobalDMAStats(uart_fd);
		if(argc == 3 && !strncmp(argv[2],"r",1)){
			ResetGlobalDMAStats(uart_fd);
		}
	}
	else if(!strncmp(argv[1],"tpc",3)){
		ReadTpcTable(uart_fd);
	}
	else{
		printf("\nInvalid option");
		printf("\n%s\n",dump_help);
	}

	return;
}

static const char *rafh_help =
	"\nUsage:"
	"\nrafh <connection handle>\n"
	"\nExample:\n"
        "\nrafh 0x15"
	"\n\n";

static void cmd_rafh(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	short int handle;

	if(argc < 2){
		printf("\n%s\n",rafh_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	handle = GetUInt(&(argv[1]),0);
	buf[0] = (handle & 0xFF);
	buf[1] = ((handle >>8) & 0xFF);
	// OGF_STATUS_PARAM 0x05
	// OCF_READ_AFH_MAP 0x0006
	iRet = writeHciCommand(uart_fd, 0x05,0x0006, 2, buf);
	if(buf[6] != 0){
		printf("\nRead AFH failed due to reason :0x%X\n",buf[6]);
		return;
	}

	if(buf[9] == 0)
		printf(" AFH is disabled");
	else
		printf(" AFH is enabled");

	handle = (buf[7] | (buf[8] << 8));
	printf("\n AFH chaneel classification for handle: 0x%X",handle);
	int i;
	printf("\n Channel Classification Map :");
	for(i=iRet-1; i>9 ; i--){
		printf("%X",buf[i]);
	}
	printf("\n");

}

static const char *safh_help =
	"\nUsage:"
	"\nsafh <host channel classification>\n"
	"\nExample:\n"
        "\nsafh 0x7FFFFFFFFFFFFFFFFFFF"
	"\n\n";

static void cmd_safh(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc < 2){
		printf("\n%s\n",safh_help);
		return;
	}
	int i,j;
	i = strlen(argv[1]);
	if(i > 20 || i < 20){
		printf("\n%s\n",safh_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	const char *map = argv[1];
	char byte[3];
	int data;
	for (i = 0,j=9; i < 20 ; i+=2,j--) {
		memcpy(byte,&map[i],2);
		byte[2] = '\0';
		data = strtol(byte, NULL, 16);
		buf[j] = (data & 0xFF);
	}
	// OGF_HOST_CTL	 0x03
	// OCF_SET_AFH_CLASSIFICATION 0x003F
	iRet = writeHciCommand(uart_fd, 0x03,0x003F,10, buf);
	if(buf[6] != 0){
		printf("\nSet AFH failed due to reason :0x%X\n",buf[6]);
		return;
	}
	printf("\nSet AFH successful!\n");
}

static const char *wotp_help =
	"\nUsage:"
	"\nwotp <address> <data> [length=1]\n"
	"\nExample:\n"
        "\nwotp 0x15 0x2020 2"
	"\n\n";

static void cmd_wotp(int uart_fd, int argc, char **argv)
{
	UINT32 address, length;

	if (argc < 3) {
		printf("\n%s\n", wotp_help);
		return;
	}
	if (argc == 4)
		length = GetUInt(&argv[3], 1);
	else
		length = 1;
	address = GetUInt(&argv[1], 0xffffffff);
	if (address == 0xffffffff) {
		printf("\n%s\n", wotp_help);
		return;
	}
	ReverseHexString(argv[2]);
	if (!write_otpRaw(uart_fd, address, length, (UCHAR *)argv[2]))
		printf("Write to OTP sucessful!\n");
}

static int write_otpRaw(int uart_fd, int address, int length, UCHAR *data)
{
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	memset(&buf, 0, MAX_EVENT_SIZE);
	buf[0] = 0x12;				/* write RAW OTP */
	buf[1] = address & 0xFF;		/* PS tag */
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = length;			/* Entry Size */
	GetByteSeq(buf + 4, data, 244);	/* Entry Data */
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS, 244 + PS_COMMAND_HEADER, buf);
	if (buf[6] != 0) {
		printf("\nWrite to OTP failed due to reason :0x%X\n", buf[6]);
		return buf[6];
	}
	return 0;
}

static const char *rotp_help =
	"\nUsage:"
	"\nrotp <address> [length=1]\n"
	"\nExample:\n"
        "\nrotp 0x15 2"
	"\n\n";

static void cmd_rotp(int uart_fd, int argc, char **argv)
{
	UINT32 address, length;
	UCHAR buf[MAX_EVENT_SIZE];

	if (argc < 2) {
		printf("\n%s\n", rotp_help);
		return;
	}
	if (argc == 3)
		length = GetUInt(&argv[2], 1);
	else
		length = 1;
	address = GetUInt(&argv[1], 0xffffffff);
	if (address == 0xffffffff) {
		printf("\n%s\n", rotp_help);
		return;
	}
	if (!read_otpRaw(uart_fd, address, length, buf))
		dumpHex(buf, length, 8);
}

static int read_otpRaw(int uart_fd, int address, int length, UCHAR *data)
{
	int iRet, plen;
	UCHAR buf[MAX_EVENT_SIZE];

	memset(&buf, 0, MAX_EVENT_SIZE);
	buf[0] = 0x11;				/* read OTP */
	buf[1] = address & 0xFF;		/* PS tag */
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = length;			/* Entry Size */
	buf[4] = 0x00;				/* Entry Data */
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_PS, 244 + PS_COMMAND_HEADER, buf);
	if (buf[6] != 0) {
		printf("\nRead from OTP failed due to reason :0x%X\n", buf[6]);
		return buf[6];
	}
	do {
		plen = read(uart_fd, buf, MAX_EVENT_SIZE);
		if (plen < 0) {
			perror("Read OTP error\n");
			exit(EXIT_FAILURE);
		}
	} while (buf[HCI_EVENT_HEADER_SIZE] != DEBUG_EVENT_TYPE_PS);
	memcpy(data, buf + HCI_EVENT_HEADER_SIZE + 1, length);
	return 0;
}

static int SU_GetId(int uart_fd, char *pStr, tSU_RevInfo *pRetRevInfo)
{
	tSU_RevInfo RevInfo;
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	RevInfo.RomVersion = 0x99999999;
	RevInfo.BuildVersion = 0x99999999;
	RevInfo.RadioFormat = 0xffff;
	RevInfo.SysCfgFormat = 0xffff;

	memset(buf, 0, MAX_EVENT_SIZE);

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_READ_VERSION, 0, buf);
	if (buf[6] != 0) {
		printf("\nRead version failed due to reason :0x%X\n", buf[6]);
		return buf[6];
	}
	RevInfo.RomVersion = buf[7] + (buf[8]<<8) + (buf[9]<<16) + (buf[10]<<24);
	RevInfo.BuildVersion = buf[11] + (buf[12]<<8) + (buf[13]<<16) + (buf[14]<<24);
	return 0;
}

/*static const char *otp_help =
	"\nUsage:"
	"\notp [dump|imp|exp|test|rpid|wpid|rvid|wvid|rba|wba|hid|cpw|cpw|pwridx|ledo] [file]\n"
	"\notp wba <BdAddr>:\n"
	"\n\n";
*/

static void cmd_otp(int uart_fd, int argc, char **argv)
{
	UCHAR buf[512], format[16];
	FILE *pF = NULL;
	UINT32 data;
	int i;

	if (argc == 1 || !strcmp(argv[1], "dump")) {
		printf("dump:\n");
		for (i = 0; i < 4; i++) {
			if (read_otpRaw(uart_fd, 128 * i, 128, &buf[128*i])) {
				printf("read failed\n");
				return;
			}
		}
		dumpHex(buf, 512, 8);
	} else if (!strcmp(argv[1], "test")) {
		printf("test:\n");
		printf("To be continue.\n");
	} else if (!strcmp(argv[1], "imp")) {
		if (argc < 3 || !*argv[2]) {
			printf("Import file content into OTP. File name is required\n");
			return;
		}
		printf("Import from %s into OTP:\n", argv[2]);
		if (!(pF = fopen(argv[2], "rb"))) {
			printf("Open file failed\n");
			return;
		}
		fread(&buf[0], sizeof(buf), 1, pF);
		fclose(pF);
		for (i = 0; i < 512; i += 4) {
			data = buf[i];
			data <<= 8;
			data += buf[i+1];
			data <<= 8;
			data += buf[i+2];
			data <<= 8;
			data += buf[i+3];
			snprintf((char *)&format, sizeof(format), "0x%08x", data);
			if (write_otpRaw(uart_fd, i, 4, (UCHAR *)format)) {
				printf("Failed!(%d)\n", i);
				return;
			}
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "exp")) {
		for (i = 0; i < 4; i++) {
			if (read_otpRaw(uart_fd, 128 * i, 128, &buf[128*i])) {

				printf("Failed\n");
				return;
			}
		}
		if (argc < 3 || !*argv[2] || (!(pF = fopen(argv[2], "wb")))) {
			/* export the content to the screen */
			dumpHex(buf, 512, 8);
		} else {
			/* export the content to the file */
			fwrite(&buf[0], sizeof(buf), 1, pF);
			fclose(pF);
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "ledo")) {
		int opendrain;
		tSU_RevInfo RevInfo;

		if (SU_GetId(uart_fd, NULL, &RevInfo))
			return;

		printf("RomVer:%02X.%02X.%02X.%02X \n", (UINT8)((RevInfo.RomVersion >> (8*3)) & 0xff),
				(UINT8)((RevInfo.RomVersion >> (8*2)) & 0xff),
				(UINT8)((RevInfo.RomVersion >> 8) & 0xff),
				(UINT8)(RevInfo.RomVersion & 0xff));
		if (((UINT8)((RevInfo.RomVersion >> (8*3)) & 0xff) == 0x01) &&
		    ((UINT8)((RevInfo.RomVersion >> (8*2)) & 0xff) == 0x02) &&
		    ((UINT8)((RevInfo.RomVersion >> 8) & 0xff) == 0x02) &&
		    ((UINT8)(RevInfo.RomVersion & 0xff) == 0x00)) {
			UINT8 LedValue[] = {0xCE, 0xDA, 0x04, 0x0C, 0x58,
					    0x04, 0x05, 0x06, 0xff, 0x50,
					    0x40, 0x01, 0x24, 0x08, 0x00,
					    0x00};
			for (opendrain = 112; opendrain < 128; opendrain++) {
				if (write_otpRaw(uart_fd, opendrain, 1, &LedValue[opendrain-112])) {
					printf("Failed\n");
					return;
				}
			}
			printf("OTP led opendrain done\n");
		} else {
			printf("Wrong RomVer\n");
		}
	} else if (!strcmp(argv[1], "cpw")) {
		UINT32 cin_value = 0, cout_value = 0;
		char tempStr[8];

		if (argc < 3) {
			printf("\n Enter cin_value : ");
			scanf("%d", &cin_value);
		} else
			cin_value = GetUInt(&argv[2], 0);
		if (cin_value < 0 || cin_value > 128) {
			printf("Invalid cin_value = %d\n", cin_value);
			return;
		}
		if (argc < 4) {
			printf("\n Enter cout_value : ");
			scanf("%d", &cout_value);
		} else
			cout_value = GetUInt(&argv[3], 0);
		if (cout_value < 0 || cout_value > 128) {
			printf("Invalid cout_value = %d\n", cout_value);
			return;
		}
		if (cout_value & 0x01) cin_value += 0x80;
		snprintf(tempStr, sizeof(tempStr), "0x%02x", cin_value);
		if (write_otpRaw(uart_fd, 4, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		snprintf(tempStr, sizeof(tempStr), "0x%02x", cout_value >> 1);
		if (write_otpRaw(uart_fd, 5, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		snprintf(tempStr, sizeof(tempStr), "0x40");
		if (write_otpRaw(uart_fd, 5, 1, (UCHAR *)tempStr)) {
			printf("CapTune Error\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "pwridx")) {
		char tempStr[8];
		snprintf(tempStr, sizeof(tempStr), "0x02");
		if (write_otpRaw(uart_fd, 21, 1, (UCHAR *)tempStr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "hid")) {
		char tempStr[8];
		UINT32 value = 0;
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter HID value(0|1) : ");
			scanf("%d", &value);
		} else
			value = GetUInt(&argv[2], 0);
		if (value != 0 && value != 1) {
			printf("\n Error: Syntax \"otp hid 0x00|0x01\"\n");
			return;
		}
		snprintf(tempStr, sizeof(tempStr), "0x%02x", value);
		if (write_otpRaw(uart_fd, 12, 1, (UCHAR *)tempStr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "wpid")) {
		UINT32 offset = 134;
		size_t len = 0;
		char pid[8] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_PID_OFFSET(default 134) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(pid, 0, sizeof(pid));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter PID : ");
			fgets((char *)pid, 7, stdin);
		} else
			strlcpy((char *)pid, argv[2], 7);
		len = strlen(pid) - 1;
		if (pid[len] == '\n' || pid[len] == '\r')
			pid[len] = 0;
		ReverseHexString(pid);
		if (write_otpRaw(uart_fd, offset, 4, (UCHAR *)pid)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rpid")) {
		UINT32 offset = 134;
		size_t len = 0;
		UCHAR Data[2];
		char *ofs = NULL;
		printf("\n Enter OTP_PID_OFFSET(default 134) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(uart_fd, offset, 2, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP PID is 0x%02x%02x\n", Data[1], Data[0]);
	} else if (!strcmp(argv[1], "wvid")) {
		UINT32 offset = 136;
		size_t len = 0;
		char vid[8] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_VID_OFFSET(default 136) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(vid, 0, sizeof(vid));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter VID : ");
			fgets(vid, 8, stdin);
		} else
			strlcpy(vid, argv[2], 7);
		len = strlen(vid) - 1;
		if (vid[len] == '\n' || vid[len] == '\r')
			vid[len] = 0;
		ReverseHexString(vid);
		if (write_otpRaw(uart_fd, offset, 2, (UCHAR *)vid)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rvid")) {
		UINT32 offset = 136;
		size_t len = 0;
		char *ofs = NULL;
		UCHAR Data[2];
		printf("\n Enter OTP_VID_OFFSET(default 136) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(uart_fd, offset, 2, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP VID is 0x%02x%02x\n", Data[1], Data[0]);
	} else if (!strcmp(argv[1], "wba")) {
		UINT32 offset = 128;
		size_t len = 0;
		char bdaddr[16] = {0};
		char *ofs = NULL;
		printf("\n Enter OTP_BDA_OFFSET(default 128) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		memset(bdaddr, 0, sizeof(bdaddr));
		if (argc < 3 || !*argv[2]) {
			printf("\n Enter BDADDR : ");
			fgets(bdaddr, 16, stdin);
		} else
			strlcpy(bdaddr, argv[2], 15);
		len = strlen(bdaddr) - 1;
		if (bdaddr[len] == '\n' || bdaddr[len] == '\r')
			bdaddr[len] = 0;
		ReverseHexString(bdaddr);
		if (write_otpRaw(uart_fd, offset, 6, (UCHAR *)bdaddr)) {
			printf("Failed\n");
			return;
		}
		printf("Done\n");
	} else if (!strcmp(argv[1], "rba")) {
		UINT32 offset = 128;
		size_t len = 0;
		char *ofs = NULL;
		UCHAR Data[6];
		printf("\n Enter OTP_BDA_OFFSET(default 128) : ");
		getline(&ofs, &len, stdin);
		sscanf(ofs, "%d", &offset);
		if (ofs) free(ofs);
		if (read_otpRaw(uart_fd, offset, 6, Data)) {
			printf("Failed\n");
			return;
		}
		printf("The OTP BDADDR is 0x%02x%02x%02x%02x%02x%02x\n",
				Data[5], Data[4], Data[3], Data[2], Data[1], Data[0]);
	}
}


static void cmd_plb(int uart_fd, int argc, char **argv)
{
	int enable, iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	if (argc < 2)
		enable = 1;
	else
		enable = GetUInt(&argv[1], 1);

	memset(buf, 0, MAX_EVENT_SIZE);
	buf[0] = 0x09;					/* audio commmand opcode */
	buf[4] = (enable == 0) ? 0x00 : 0x01;		/* audio command param */
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_AUDIO_CMD, 8, buf);
	if (buf[6] != 0) {
		printf("\nError in setting PCM CODEC loopback :0x%X\n", buf[6]);
		return;
	}
	printf("\nPCM CODEC loopback is %s\n", (enable == 0) ? "OFF" : "ON");

}


static void cmd_psw(int uart_fd, int argc, char **argv)
{
	int enable, freq, iRet;
	UCHAR buf[MAX_EVENT_SIZE];
	if (argc < 2) {
		enable = 1;
		freq = 440;
	}
	else if (argc < 3) {
		printf("aa\n");
		enable = GetUInt(&argv[1], 1);
		freq = 440;
	} else {
		enable = GetUInt(&argv[1], 1);
		freq = GetUInt(&argv[2], 440);
	}
	if (freq > 3700) {
		printf("Invalid frequency. It should be in the range of 0 to 3700\n");
		return;
	}

	memset(buf, 0, MAX_EVENT_SIZE);
	buf[0] = 0x0a;					/* audio command opcode */
	buf[4] = (enable == 0) ? 0x00 : 0x01;		/* audio command param */
	buf[5] = 0x00;
	buf[6] = freq & 0xff;
	buf[7] = (freq >> 8) & 0xff;

	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, OCF_AUDIO_CMD, 8, buf);
	if (buf[6] != 0) {
		printf("\nError in running PCM sine wave playback :0x%X\n", buf[6]);

		return;
	}
	printf("PCM CODEC PCM sine wave playback is %s\n", (enable == 0) ? "OFF" : "ON");

}

static const char *lert_help=
	"\nUsage:"
	"\nlert <rx_channel>\n"
	"\nlert 30 \n"
	"\n\n";

static void cmd_lert(int uart_fd, int argc, char **argv)
{
	UCHAR channel;
	if (argc < 2) {
		printf("\n%s\n", lert_help);
		return;
	}
	channel = (UCHAR)GetUInt(&argv[1], 0);

	SU_LERxTest(uart_fd, channel);

}

static BOOL SU_LERxTest(int uart_fd, UCHAR channel)
{
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if (channel < MB_MIN_FREQUENCY_LE || channel > MB_MAX_FREQUENCY_LE) {
		printf("Invalid rx channel. It should be in the range of %d to %d\n",
			MB_MIN_FREQUENCY_LE, MB_MAX_FREQUENCY_LE);
		return FALSE;
	}

	memset(buf, 0, MAX_EVENT_SIZE);
	buf[0] = channel;			/* rx_channel */
	// OGF_LE_CTL 0x08
	// OCF_LE_RECEIVER_TEST	0x001D
	iRet = writeHciCommand(uart_fd, 0x08, 0x001D, 1, buf);
	if (buf[6] != 0) {
		printf("\nError in putting the device into LE RX mode\n");
		return FALSE;
	}
	return TRUE;
}

static const char *lett_help=
	"\nUsage:"
	"\nlett <rx_channel> <length> <packet_payload>\n"
	"\nlett 30 30 5\n"
	"\n\n";

static void cmd_lett(int uart_fd, int argc, char **argv)
{
	UCHAR channel, length, payload;
	if (argc < 4) {
		printf("\n%s\n", lett_help);
		return;
	}
	channel = (UCHAR)GetUInt(&argv[1], 0);
	length = (UCHAR)GetUInt(&argv[2], 0);
	payload = (UCHAR)GetUInt(&argv[3], 0);


	SU_LETxTest(uart_fd, channel, length, payload);

}

static BOOL SU_LETxTest(int uart_fd, UCHAR channel, UCHAR length, UCHAR payload)
{
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if (channel < MB_MIN_FREQUENCY_LE || channel > MB_MAX_FREQUENCY_LE) {
		printf("Invalid tx channel. It should be in the range of %d to %d\n",
			MB_MIN_FREQUENCY_LE, MB_MAX_FREQUENCY_LE);
		return FALSE;
	}
	if (length < MB_MIN_DATALEN_LE || length > MB_MAX_DATALEN_LE) {
		printf("Invalid data length. It should be in the range of %d to %d\n",
			MB_MIN_DATALEN_LE, MB_MAX_DATALEN_LE);
		return FALSE;
	}
	if (payload > 7) {
		printf("Invalid packet payload. It should be in the range of 0 to 7\n");
		return FALSE;
	}

	memset(buf, 0, MAX_EVENT_SIZE);
	buf[0] = channel;			/* tx_channel */
	buf[1] = length;			/* length of test data */
	buf[2] = payload;			/* packet payload */
	// OGF_LE_CTL 0x08
	// OCF_LE_TRANSMITTER_TEST 0x001E
	iRet = writeHciCommand(uart_fd, 0x08, 0x001E, 3, buf);
	if (buf[6] != 0) {
		printf("\nError in putting the device into LE TX mode\n");
		return FALSE;
	}
	return TRUE;
}


static void cmd_lete(int uart_fd, int argc, char **argv)
{
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	memset(buf, 0, MAX_EVENT_SIZE);
	// OGF_LE_CTL 0x08
	// OCF_LE_TEST_END 0x001F
	iRet = writeHciCommand(uart_fd, 0x08, 0x001F, 0, buf);
	if (buf[6] != 0) {
		printf("\nError in ending LE test\n");
		return;
	}
	printf("Number of packets = %d\n", buf[7] | (buf[8] << 8));

}

static const char *tputs_help =
	"\nUsage:"
	"\ntput-s [BD_Addr] [Judgment value] Logfile times"
	"\ntput-s 11:22:33:44:55:66 150 log.txt 10"
	"\n\n";

static void CalculateTput(int uart_fd, UINT16 hci_handle, char *filename, double threshold, int tx_size)
{
	time_t start, finish, checkbreak;
	UCHAR buf[1009];
	FILE *fp = NULL;
	int aclnum = 8;
	int retval;
	unsigned long sentnum = 0;
	double TimeResult = 0;
	fd_set rfds;
	struct timeval tv1, tv2, timeout;
	unsigned long long start_utime, end_utime, time_diff;
	unsigned long long throughput;

	start = time(NULL);
	gettimeofday(&tv1, NULL);
	start_utime = tv1.tv_sec*1000000 + tv1.tv_usec;
	while (sentnum < 1024 * tx_size) {
		while (aclnum > 0) {
			aclnum--;
			buf[0] = 0x02; // HCI_ACLDATA_PKT 0x02
			/* ACL packet header */
			buf[1] = hci_handle & 0xFF;
			buf[2] = ((hci_handle >> 8) & 0x0E);
			buf[3] = 1004 & 0xff;
			buf[4] = (1004 >> 8) & 0xff;
			/* L2CAP packet header */
			buf[5] = 1000 & 0xff;
			buf[6] = (1000 >> 8) & 0xff;
			buf[7] = 0x40 & 0xff;
			buf[8] = 0;

			memset(buf+9, sentnum++, 1000);
			while (write(uart_fd, (const void *)buf, 1009) < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				perror("HCI send packet failed");
				exit(EXIT_FAILURE);
			}
		}
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		FD_ZERO(&rfds);
		FD_SET(uart_fd, &rfds);
		retval = select(uart_fd+1, &rfds, NULL, NULL, &timeout);
		if (retval == -1) {
			perror("select()");
			exit(EXIT_FAILURE);
		} else if (retval) {
			/* Data is available now */
			ssize_t plen;
			UCHAR buffer[64];
			int i;
			plen = read(uart_fd, buffer, 64);
			if (plen < 0) {
				perror("HCI read buffer failed");
				exit(EXIT_FAILURE);
			}
			for (i = 0; i < buffer[HCI_EVENT_HEADER_SIZE]; i++)
				aclnum += (buffer[HCI_EVENT_HEADER_SIZE+(i+1)*2+1] | (buffer[HCI_EVENT_HEADER_SIZE+(i+1)*2+2] << 8));
		}
		checkbreak = time(NULL);
		if ((checkbreak - start) >= 300) break;
	}
	finish = time(NULL);
	gettimeofday(&tv2, NULL);
	end_utime = tv2.tv_sec*1000000 + tv2.tv_usec;
	time_diff = end_utime - start_utime;
	throughput = time_diff/1000;
	throughput = (sentnum * 1000)/throughput;
	printf("Transfer Completed! throughput [%0d KB/s]", (int)throughput);
	printf(" result [%s]\n", threshold > throughput ? " Fail " : " Pass ");
	if (filename && *filename)
		fp = fopen(filename, "at+");
	if (fp) {
		fprintf(fp, "Transfer Completed! throughput [%.0f KB/s]", TimeResult);
		fprintf(fp, " result [%s]\n", threshold > TimeResult ? " Fail " : " Pass ");
		fclose(fp);
	}
}

static void cmd_tputs(int uart_fd, int argc, char **argv)
{

#if 0
	int j, iRet, loop = 1, tx_test_size = 1;
	UINT16 Ps_EntrySize = 0;
	UINT16 hci_handle = 0;
	double threshold = 0.0;
	char *filename = NULL;
	struct sigaction sa;
	FILE *fp = NULL;
	bdaddr_t bdaddr;
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR Ps_Data[MAX_EVENT_SIZE];
	UINT16 *pPs_Data;
	BOOL Ok = FALSE;
	char timeString[9] = {0};
	char dateString[15] = {0};
	time_t current_time;
	struct tm *time_info;
	tSU_RevInfo RevInfo;

	if (argc < 3) {
		printf("\n%s\n", tputs_help);
		return;
	}

	if (str2ba(argv[1],&bdaddr)) {
		printf("\nPlease input valid bdaddr.\n");
		return;
	}
	threshold = atof(argv[2]);
	if (!threshold) {
		printf("\nPlease input valid throughput threshold.\n");
		return;
	}
	if (argc > 3)
		filename = strdup(argv[3]);
	if (argc > 4)
		loop = GetUInt(&argv[4], 1);
	if (argc > 5)
		tx_test_size = GetUInt(&argv[5],1);

	CtrlCBreak = FALSE;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	PSInit(uart_fd);
	memset(buf, 0, sizeof(buf));
	iRet = writeHciCommand(uart_fd, OGF_HOST_CTL, OCF_RESET, 0, buf);
	if (buf[6] != 0) {
		printf("Error: HCI RESET failed.\n");
		return;
	}
	sleep(1);
	for (j = 0; j < loop; j++) {
		int i = 0;
		if (!j) sleep(1);
		printf("\n-----------------------------------");
		printf("\nTimes %d/%d\n", j + 1, loop);

		time(&current_time);
		time_info = localtime(&current_time);
		strftime(timeString, sizeof(timeString), "%H %M %S", time_info);
		strftime(dateString, sizeof(dateString), "%b %d %Y", time_info);
		if (j == 0) {
			if (filename && *filename)
				fp = fopen(filename, "at+");
			if (fp != NULL)
				fprintf(fp, "\n[%s %s] \nCMD : TPUT-S %s %f %s %d\n",
						dateString, timeString, argv[1], threshold, filename, loop);
			/* SFLAGS FW */
			Ok = PSOperations(uart_fd, PS_GET_LENGTH, PSTAG_RF_TEST_BLOCK_START, (UINT32 *)&Ps_EntrySize);
			if (Ok) {
				Ps_Data[0] = Ps_EntrySize & 0xff;
				Ps_Data[1] = (Ps_EntrySize >> 8) & 0xff;
				Ok = PSOperations(uart_fd, PS_READ, PSTAG_RF_TEST_BLOCK_START, (UINT32 *)&Ps_Data);
				if (Ok) {
					pPs_Data = (UINT16 *)&Ps_Data[0];
					if (*pPs_Data == BT_SOC_INIT_TOOL_START_MAGIC_WORD) {
						RevInfo.RadioFormat = *(pPs_Data + 1);
						RevInfo.RadioContent = *(pPs_Data + 2);
					}
				}
			}

			/* Get syscfg info */
			Ok = PSOperations(uart_fd, PS_GET_LENGTH, PSTAG_SYSCFG_PARAM_TABLE0, (UINT32 *)&Ps_EntrySize);
			if (Ok) {
				Ps_Data[0] = Ps_EntrySize & 0xff;
				Ps_Data[1] = (Ps_EntrySize >> 8) & 0xff;
				Ok = PSOperations(uart_fd, PS_READ, PSTAG_SYSCFG_PARAM_TABLE0, (UINT32 *)&Ps_Data);
				if (Ok) {
					pPs_Data = (UINT16 *)&Ps_Data[0];
					if (*pPs_Data == 0xC1C1) {
						RevInfo.SysCfgFormat = *(pPs_Data + 1);
						RevInfo.SysCfgContent = *(pPs_Data + 2);
					}

				}
			}

			if (RevInfo.SysCfgFormat != 0xff) {
				printf("SysCfg -    Format:  %d.%d\n",((RevInfo.SysCfgFormat >> 4) & 0xfff), (RevInfo.SysCfgFormat & 0xf));
				printf("            Content: %d\n", RevInfo.SysCfgContent);
				if (fp) {
					fprintf(fp, "SysCfg -    Format:  %d.%d\n",((RevInfo.SysCfgFormat >> 4) & 0xfff),
							(RevInfo.SysCfgFormat & 0xf));
					fprintf(fp, "            Content: %d\n", RevInfo.SysCfgContent);
				}
			} else {
				printf("SysCfg - N/A\n");
				if(fp)
					fprintf(fp, "SysCfg - N/A\n");
			}

			/* bd addr */
			memset(&buf, 0, sizeof(buf));
			iRet = writeHciCommand(uart_fd, OGF_INFO_PARAM, OCF_READ_BD_ADDR, 0, buf);
			if (buf[6] != 0) {
				printf("\nCould not read the BD_ADDR (time out)\n");
			} else {
				char temp[16] = {0};
				memset(temp, 0, sizeof(temp));
				snprintf(temp, sizeof(temp), "%02X%02X%02X%02X%02X%02X", buf[iRet-1], buf[iRet-2],
						buf[iRet-3], buf[iRet-4], buf[iRet-5], buf[iRet-6]);
				printf("\nLocal BDAddress : 0x%s\n", temp);
				if (fp)
					fprintf(fp, "Local BDAddress : 0x%s\n", temp);
			}

			if (fp) {
				fclose(fp);
				fp = NULL;
			}
		}
		printf("Sending packages to 0x%s\n", argv[1]);
		while (i++ < 3) {
			iRet = hci_create_connection(uart_fd, &bdaddr, 0xCC18, 0, 0, &hci_handle, 0);
			if (!iRet || CtrlCBreak) break;
		}

		if (iRet) {
			if (filename && *filename) {
				fp = fopen(filename, "at+");
				if (fp) {
					fprintf(fp, "Transfer Failed! \n");
					fclose(fp);
					fp = NULL;
				}
			}
			printf("Transfer Failed! \n");
			CtrlCBreak = TRUE;

			return;
		}
		CalculateTput(uart_fd, hci_handle, filename, threshold, tx_test_size);

		hci_disconnect(uart_fd, hci_handle, 0, 30);

		if (CtrlCBreak) break;
        }
	CtrlCBreak = TRUE;
#endif
}

static void cmd_tputr(int uart_fd, int argc, char **argv)
{
	int iRet;
	ssize_t plen;
	UINT16 hci_handle = 0;
	UCHAR buf[MAX_EVENT_SIZE];
	struct sigaction sa;

	CtrlCBreak = FALSE;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	memset(buf, 0, sizeof(buf));
	// OGF_HOST_CTL 0x03
	// OCF_RESET 0x0003
	iRet = writeHciCommand(uart_fd, 0x03, 0x0003, 0, buf);
	if (buf[6] != 0) {
		printf("Error: HCI RESET failed.\n");
		return;
	}
	sleep(1);
	memset(buf, 0, sizeof(buf));
	buf[0] = 0x02;
	// OGF_HOST_CTL	 0x03
	// OCF_WRITE_SCAN_ENABLE 0x001A
	iRet = writeHciCommand(uart_fd, 0x03, 0x001A, 1, buf);
	if (buf[5] != 0) {
		printf("Error: Write scan failed\n");
		return;
	}
	printf("Start listening ...\n");
	do {
		plen = read(uart_fd, buf, MAX_EVENT_SIZE);
		if (plen < 0) {
			printf("reading failed...\n");
			if (errno == EAGAIN || errno == EINTR) continue;
			else {
				perror("HCI read failed");
				exit(EXIT_FAILURE);
			}
		}
		// EVT_CONN_REQUEST 0x04
		if (buf[1] == 0x04) {
			int i, j;
			ssize_t plen = 0;
			printf("Connection come in\n");
			for (i = 0, j = 3; i < BD_ADDR_SIZE; i++, j++)
				buf[i] = buf[j];
			buf[BD_ADDR_SIZE] = 0x01;
			// OGF_LINK_CTL 0x01
			// OCF_ACCEPT_CONN_REQ 0x0009
			if (hci_send_cmd(uart_fd, 0x01, 0x0009, 7, buf)) {
				printf("Accept connection error\n");
				return;
			}
			do {
				plen = read(uart_fd, buf, MAX_EVENT_SIZE);
				if (plen < 0) {
					perror("Read failed");
					exit(EXIT_FAILURE);
				}
			// EVT_CONN_COMPLETE 0x03
			} while (buf[1] != 0x03);
			if (buf[3] == 0) {
				printf("Connection up\n");
			} else {
				printf("Connection failed\n");
			}
			hci_handle = (buf[4] | (buf[5] << 8)) & 0x0EFF;
		// EVT_DISCONN_COMPLETE 0x05
		} else if (buf[1] == 0x05) {
			UINT16 hdl = buf[4] | (buf[5] << 8);
			printf("Disconnect...\n");
			if (hdl == hci_handle) {
				break;
			}
		} else if (CtrlCBreak) {
			printf("CtrlBreak...\n");
			break;
		}
	} while (plen >= 0);
	CtrlCBreak = TRUE;

}

int sock_recv(int sockid, unsigned char *buf, int buflen)
{
    int recvbytes;
    recvbytes = recv(sockid, buf, buflen, 0);
    if (recvbytes == 0) {
        printf("Connection close!? zero bytes received\n");
        return -1;
    } else if (recvbytes > 0) {
        return recvbytes;
    }
    return -1;
}

int sock_send(int sockid, unsigned char *buf, int bytes)
{
    int cnt;
    unsigned char* bufpos = buf;
    while (bytes) {
        cnt = write(sockid, bufpos, bytes);
	if (cnt != bytes)
		printf("cnt:%d,bytes:%d\n",cnt, bytes);

        if (!cnt) {
            break;
        }
        if (cnt == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }

        bytes -= cnt;
        bufpos += cnt;
    }
    return (bufpos - buf);
}

static void cmd_btagent(int uart_fd, int argc, char **argv)
{
    int i, j, k, l, iRet, rx_enable, iDataSize, m;
    uint32_t m_BerTotalBits, m_BerGoodBits;
    uint8_t m_pattern[16];
    uint16_t m_pPatternlength;
    int port = BT_PORT;
    struct sigaction sa;
    unsigned char buf[1024];
    struct timeval timeout;

/* master file descriptor list */
    fd_set master;
    fd_set read_fds;

/* server address */
    struct sockaddr_in serveraddr;

    int fdmax;

/* listening socket descriptor */
    int listener = -1;

/* newly accept()ed socket descriptor */
    int newfd = -1;

    int nbytes;

/* for setsockopt() SO_REUSEADDR, below */
    int yes = 1;

    int addrlen;

    if (argc > 1)
	port = atoi(argv[1]);
    if (port == 0)
	port = BT_PORT;
    else if (port < 0 || port >65534) {
	perror("\nERROR: Invalid port number\n");
	return;
    }

    CtrlCBreak = FALSE;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_term;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* clear the master and temp sets */
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /* get the listener */
    if((listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("Server-socket() error lol!");
		return;
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int)) == -1) {
		perror("Server-setsockopt() error lol!");
		close(listener);
		return;
    }

    if(setsockopt(listener, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int)) == -1) {
	perror("Server-setsockopt() error TCP_NODELAY\n");
        close(listener);
        return;
    }

    /* bind */
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    memset(&(serveraddr.sin_zero), 0, 8);

    if(bind(listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("Server-bind() error lol!");
		close(listener);
		return;
    }

    /* listen */
    if(listen(listener, 10) == -1) {
	perror("Server-listen() error lol!");
	close(listener);
	return;
    }

    /* add the listener to the master set */
    FD_SET(listener, &master);

    /* add hci handler to the master set */
    FD_SET(uart_fd, &master);

    FD_SET(0, &master);
    /* keep track of the biggest file descriptor */
    fdmax = listener;
    if (uart_fd > listener) fdmax = uart_fd;

    printf("Start BtAgent, press 'q' to exit.\n");

    rx_enable = 0;
    m_BerGoodBits = 0;
    m_BerTotalBits = 0;
    m_pattern[0] = 0x0f;
    m_pPatternlength = 1;

    while (1) {
		read_fds = master;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		iRet = select(fdmax+1, &read_fds, NULL, NULL, &timeout);
	if (iRet == -1) {
		perror("Server-select() error lol!");
		if (newfd > 0) close(newfd);
	    close(listener);
	    goto exits;
        }
	if (CtrlCBreak) break;
	if (iRet == 0) continue;

		/*run through the existing connections looking for data to be read*/
		for(i = 0; i <= fdmax; i++) {
			if(FD_ISSET(i, &read_fds)) {

		if(i == 0) {
	            printf("Shutting down btagent\n");
                    iRet = getchar();
		    if (iRet == 'q') goto exits;
		    continue;
		}

		if(i == listener) {
                    /* handle new connections */
                    addrlen = sizeof(struct sockaddr_in);
                    if((newfd = accept(listener, (struct sockaddr *)&serveraddr, &addrlen)) == -1) {
                        perror("Server-accept() error lol!");
			goto exits;
                    }
                    else {
                        printf("Server-accept() is OK...%d\n",newfd);
                        FD_SET(newfd, &master); /* add to master set */
                        if(newfd > fdmax)
                             fdmax = newfd;
		    }
                }
                else if (i == newfd) {
                    /* handle data from a client */
                    if((nbytes = sock_recv(i, buf, sizeof(buf))) < 0) {
						/* got error or connection closed by client */
                        close(i);
                        /* remove from master set */
                        FD_CLR(i, &master);
                    }
                    else {

					for (j=0; j<nbytes; j++)
                            printf("%x ",buf[j]);
					printf("\n");
			if (buf[0] == 0x7) {	// BTAGENT_CMD_EVENT
			    if (buf[3] == 0x01) {	// BTAGENT_CMD_EVENT_GETBER
					buf[11] = (m_BerTotalBits & 0xff000000) >> 24;
					buf[10] = (m_BerTotalBits & 0xff0000) >> 16;
					buf[9] = (m_BerTotalBits & 0xff00) >> 8;
					buf[8] = m_BerTotalBits & 0xff;
					buf[7] = (m_BerGoodBits & 0xff000000) >> 24;
					buf[6] = (m_BerGoodBits & 0xff0000) >> 16;
					buf[5] = (m_BerGoodBits & 0xff00) >> 8;
					buf[4] = m_BerGoodBits & 0xff;
					buf[3] = 1;		// BTAGENT_CMD_EVENT_GETBER
					buf[2] = 0;
					buf[1] = 9;
					buf[0] = 7;
					sock_send(newfd, buf, 9+3);
					usleep(2000);
			    }
			    else if (buf[3] == 0x02) {	// BTAGENT_CMD_EVENT_PATTERN
				m_pPatternlength = (uint16_t)(buf[1] | (buf[2] << 8));
				m_pPatternlength --;
				if (m_pPatternlength > 16) m_pPatternlength = 16;
				memcpy(m_pattern,&buf[4],m_pPatternlength);
				printf("PatternLength:%d,%x\n",m_pPatternlength,buf[4]);
			    }
			    continue;
                        }

			if (rx_enable == 1) {
			    if ((buf[4] == 0x03) && (buf[5] == 0x0c))
				rx_enable = 0;
			}
			write(uart_fd, &buf[3], nbytes - 3);
		    }
		}
		else if (i == uart_fd) {
                    nbytes = read(uart_fd, &buf[3], sizeof(buf) - 3);
		    iDataSize = nbytes - 6;
//		    printf("nbyte:%d, packet:%d, pattern:%x\n",nbytes, (uint16_t)(buf[6] | (buf[7] << 8)), buf[8]);
		    if (buf[3] == 0x2) {	// ACL data
				if (rx_enable) {
					m_BerTotalBits = m_BerTotalBits + iDataSize * 8;
					for(j=0,l=0;j<iDataSize;j++,l++) {
					if (l == m_pPatternlength) l = 0;
						for(k=0;k<8;k++){
							if((m_pattern[l]&(1<<k)) == (buf[8+j]&(1<<k)))
							m_BerGoodBits++;
						}
					}
				}
		    }
		    else {
		        if ((buf[7] == 0x5b) && (buf[8] == 0xfc)) {		// Rx start CMD's event
			    rx_enable = 1;
			    m_BerTotalBits = 0;
			    m_BerGoodBits = 0;
				}
				buf[2] = 0;
				buf[1] = (uint16_t)nbytes;
				buf[0] = 3;
				if (newfd > 0) {
					sock_send(newfd, buf, nbytes+3);
					usleep(2000);
				}
		    }
		}
	    }
        }
    }
exits:

    if (listener > 0) close(listener);
    if (newfd > 0) close(newfd);
    printf("Total:%d,Good:%d\n",m_BerTotalBits, m_BerGoodBits);
}

/* SMD cmds */
static const char *hciinq_help =
	"Usage:\n"
	"\n hciinq\n";

static void cmd_hciinq(int uart_fd, int argc, char **argv){
	int iRet, i;
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR resultBuf[MAX_EVENT_SIZE];
	unsigned long val32; 

	printf( "inq command ++ argc = %d argv = %s %s %s %s %s \n", argc,argv[1],argv[2] ,argv[3] ,argv[4] ,argv[5]  );
	if(argc < 2){
		printf("\n%s\n",hciinq_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	// INQUIRY_CP_SIZE 5
	for (i = 1; i <= 5; i++)
	{
		UCHAR *tmp ; 
		val32 = strtol ((char*)argv[i], NULL, 16);
		tmp = ((unsigned char*)&val32); 
		buf[i-1] = *tmp; 
	} 



	// OGF_LINK_CTL 0x01
	// OCF_INQUIRY 0x0001
	// INQUIRY_CP_SIZE 5
	iRet = hci_send_cmd( uart_fd, 0x01, 0x0001, 5, buf);

	printf("\nINQUIRY: \n");
	memset(&resultBuf,0,MAX_EVENT_SIZE);
	if (!iRet)
	 	read_incoming_events(uart_fd, resultBuf, 0);
		
	printf("\n");
}

static const char *hciinqcnl_help =
	"Usage:\n"
	"\n hciinqcnl\n";

static void cmd_hciinqcnl(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc > 1){
		printf("\n%s\n",hciinqcnl_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	// OGF_LINK_CTL 0x01
	// OCF_INQUIRY_CANCEL 0x0002
	iRet = writeHciCommand(uart_fd, 0x01, 0x0002, 0, buf);
	if(buf[6] != 0){
		printf("\nError: Inquiry cancel failed due to the reason 0X%X\n", buf[6]);
		return;
	}
	printf("\nINQUIRY CANCEL\n");
}

static const char *hcisetevtflt_help =
	"Usage:\n"
	"\n hcisetevtflt\n";

static void cmd_hcisetevtflt(int uart_fd, int argc, char **argv){
	int iRet, i;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc < 2){
		printf("\n%s\n",hcisetevtflt_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	// SET_EVENT_FLT_CP_SIZE 2
	for (i = 0; i < 2; i++)
		buf[i] = atoi(argv[i + 1]);
	// OGF_HOST_CTL	 0x03
	// OCF_SET_EVENT_FLT 0x0005
	iRet = writeHciCommand(uart_fd, 0x03, 0x0005,
			2, buf);
	if(buf[6] != 0){
		printf("\nError: Set Event Filter failed due to the reason 0X%X\n", buf[6]);
		return;
	}
	printf("\nSet Event Filter\n");
}

static const char *pinconntest_help =
	"Usage:\n"
	"\n pinconntest\n";

static void cmd_pinconntest(int uart_fd, int argc, char **argv){
	int iRet, i;
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR resultBuf[MAX_EVENT_SIZE];
    unsigned long val32; 
	if(argc < 2){
		printf("\n%s\n",pinconntest_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
        UCHAR *tmp ; 
	val32 = strtol ((char*)argv[1], NULL, 16);
	tmp = ((unsigned char*)&val32); 
	buf[0] =*tmp; 
	iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x0C, 1, buf);

	printf("\nPIN CONNECTIVITY TEST: \n");
	memset(&resultBuf,0,MAX_EVENT_SIZE);
	if (!iRet)
	 	read_incoming_events(uart_fd, resultBuf, 0);
		
	printf("\n");
}

static const char *conntest_help =
	"Usage:\n"
	"\n conn < bdaddress >\n";

	static void cmd_createconnection(int uart_fd, int argc, char **argv){
		int iRet, i,j;
		UCHAR buf[MAX_EVENT_SIZE];
		UCHAR resultBuf[MAX_EVENT_SIZE];
		bdaddr_t bdaddr;
		uint16_t handle;
		uint8_t role;
		unsigned char ptype[2] = {0x18 , 0xcc};
		int dd, opt;

		role = 0x01;


		if(argc < 2){
			printf("\n%s\n",conntest_help);
			return;
		}
		str2ba(argv[1],&bdaddr);
		if((strlen(argv[1]) < 17)||(strlen(argv[1]) > 17)){
			printf("\nInvalid BD address : %s\n",argv[1]);
			printf("\n%s\n",wba_help);
			return;
		}


		memset(&buf,0,MAX_EVENT_SIZE);

		for(i= 0,j=0; i< BD_ADDR_SIZE;i++,j++){
			buf[j] = bdaddr.b[i];
		}
		buf[6] = ptype[0];
		buf[7] = ptype[1];
		buf[8] = 0x00;
		buf[9] = 0x00;
		buf[10] = 0x00;
		buf[11] = 0x00;
		buf[12] = 0x01; 


		// OGF_LINK_CTL 0x01
		// OCF_CREATE_CONN 0x0005
		iRet = hci_send_cmd( uart_fd, 0x01, 0x0005,13 , buf);
		printf("\n Connect test \n");
		for(i = 0; i < 1;i++){
			printf("%02X:",buf[i]);
		}
	
		memset(&resultBuf,0,MAX_EVENT_SIZE);
		if (!iRet)
			read_incoming_events(uart_fd, resultBuf, 0);



	}


static const char *disc_help =
	"Usage:\n"
	"\n disc <handle in 2 octets Hex><reason in hex>";

static void cmd_disc(int uart_fd, int argc, char **argv){
	int iRet;
        
	unsigned long val32; 
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR resultBuf[MAX_EVENT_SIZE];
	if(argc < 3){
		printf("\n%s\n",disc_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
        
	
 
	val32 = strtol ((char*)argv[1], NULL, 16);
	buf[0] = val32 & 0xff ;
	buf[1] = (val32 & 0xff00) >>8;
       
	val32 = strtol ((char*)argv[2], NULL, 16);
	buf[2] = *(UCHAR *)&val32;
	printf("\nHCI_Disconnect: Handle :%s Reason Code: 0x%x\n",argv[1],buf[2]);
	// OGF_LINK_CTL 0x01
	// OCF_DISCONNECT 0x0006
	iRet = hci_send_cmd(uart_fd, 0x01, 0x0006 ,3 , buf);


	memset(&resultBuf,0,MAX_EVENT_SIZE);
	if (!iRet)
	 	read_incoming_events(uart_fd, resultBuf, 0);
		
	printf("\n");

}


static const char *venspeccmd_help =
	"Usage:\n"
	"\n venspeccmd [3|6]\n";

static void cmd_venspeccmd(int uart_fd, int argc, char **argv){
	int iRet,i;
	UCHAR buf[MAX_EVENT_SIZE];
        UCHAR resultBuf[MAX_EVENT_SIZE];
        unsigned long val32;
	if(argc < 2){
		printf("\n%s\n",venspeccmd_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
       	for (i = 1; i < argc; i++)
	    {
		UCHAR *tmp ; 
		val32 = strtol ((char*)argv[i], NULL, 16);
		tmp = ((unsigned char*)&val32); 
		buf[i-1] = *tmp; 
	    } 

        iRet = hci_send_cmd( uart_fd, HCI_VENDOR_CMD_OGF, 0x00, i-1, buf);

        printf("Vendor Specific command\n");
        memset(&resultBuf,0,MAX_EVENT_SIZE);
        if (!iRet)
                read_incoming_events(uart_fd, resultBuf, 0);
        printf("\n");
}

static const char *rawcmd_help =
	"Usage:\n"
	"\n rawcmd ogf ocf <bytes> \n";

static void cmd_rawcmd(int uart_fd, int argc, char **argv){
	int iRet,i,j;
	UCHAR buf[MAX_EVENT_SIZE];
	UCHAR resultBuf[MAX_EVENT_SIZE];
	uint16_t ogf, ocf;
	unsigned long val32;
	unsigned char *pval8;
	
	if(argc < 2){
		printf("\n%s\n",rawcmd_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);

	val32 = strtol((char*)argv[1], NULL, 16);
	pval8 = ((unsigned char*)&val32);
	ogf = (unsigned short)*pval8;

	val32 = strtol((char*)argv[2], NULL, 16);
	pval8 = ((unsigned char*)&val32);
	ocf = (unsigned short)*pval8;
	
	for (i = 3; i < argc; i++)
	{
		UCHAR *tmp ; 
		val32 = strtol ((char*)argv[i], NULL, 16);
		tmp = ((unsigned char*)&val32); 
		buf[i-3] = *tmp; 
	} 

	printf("ogf 0x%x ocf 0x%x\n Params: ", ogf, ocf);

	for (j = 0; j < i-3; j++) {
		printf("0x%x ", buf[j]);
	}
	printf("\n");

	iRet = hci_send_cmd( uart_fd, ogf, ocf, i-3, buf);

	printf("Raw HCI command\n");
	memset(&resultBuf,0,MAX_EVENT_SIZE);
	//if (!iRet)
	//	read_incoming_events(uart_fd, resultBuf, 0);
	printf("\n");
}

static const char *hciinvcmd1_help =
	"Usage:\n"
	"\n hciinvcmd1\n";

static void cmd_hciinvcmd1(int uart_fd, int argc, char **argv){
	int iRet;
	UCHAR buf[MAX_EVENT_SIZE];

	if(argc > 1){
		printf("\n%s\n",hciinvcmd1_help);
		return;
	}

	memset(&buf,0,MAX_EVENT_SIZE);
	buf[0] = atoi(argv[1]);
	iRet = writeHciCommand(uart_fd, HCI_VENDOR_CMD_OGF, 0x00, 1, buf);
	if(buf[6] != 0){
		printf("\nError: Invalid HCI cmd due to the reason 0X%X\n", buf[6]);
		return;
	}
	printf("\nINVALID HCI COMMAND: \n");
	int i;
	for(i=iRet-1;i > 7;i--){
		printf("%02X:",buf[i]);
	}
	printf("%X \n\n",buf[7]);
}

/* EOF SMD cmds */
static void sig_term(int sig)
{
	if (CtrlCBreak) return;
	CtrlCBreak = TRUE;
}
  
static struct {
	char *cmd;
	char *cmd_option;
	void (*func)(int uart_fd, int argc, char **argv);
	char *doc;
} command[] = {
	{ "reset","      ",   cmd_reset,    "Reset Target"                },
	{ "rba","       ",  cmd_rba,    "Read BD Address"                },
	{ "wba","<bdaddr> ",   cmd_wba,    "Write BD Address"                },
	{ "edutm","       ",  cmd_edutm,    "Enter DUT Mode"                },
	{ "wsm","<mode>  ",   cmd_wsm,    "Write Scan Mode"                },
	{ "mb","       ",   cmd_mb,    "Enter Master Blaster Mode"                },
	{ "mbr","<address> <length>  ",   cmd_mbr,    "Block memory read"                },
	{ "peek","<address> <width>  ",   cmd_peek,    "Read Value of an Address"                },
	{ "poke","<address> <value> <mask> <width>  ",   cmd_poke,    "Write Value to an Address"                },
	{ "cwtx","<channel number> ",   cmd_cwtx,    "Enter Continuous wave Tx"                },
	{ "cwrx","<channel number> ",   cmd_cwrx,    "Enter Continuous wave Rx"                },
	{ "rpst","<length> <id>  ",   cmd_rpst,    "Read PS Tag"                },
	{ "wpst","<length> <id> <data> ",   cmd_wpst,    "Write PS Tag"                },
	{ "psr","       ",   cmd_psr,    "PS Reset"                },
	{ "setap","<storage medium> <priority>",   cmd_setap,    "Set Access Priority"                },
	{ "setam","<storage medium> <access mode>",   cmd_setam,    "Set Access Mode"               },
	{ "rpsraw","<offset> <length>  ",   cmd_rpsraw,    "Read Raw PS"                },
	{ "wpsraw","<offset> <length>  <data>",   cmd_wpsraw,    "Write Raw PS"                },
	{ "ssm","<disable|enable>         ", cmd_ssm, "Set Sleep Mode"      },
	{ "dtx","         ", cmd_dtx, "Disable TX"      },
	{ "dump","<option>         ", cmd_dump, "Display Host Controller Information"      },
	{ "rafh","<connection handle>         ", cmd_rafh, "Read AFH channel Map"      },
	{ "safh","<channel classification>         ", cmd_safh, "Set AFH Host Channel Classification"      },
	{ "wotp", "<address> <data> [length=1]", cmd_wotp, "Write Length (default 1) bytes of Data to OTP started at Address"      },
	{ "rotp", "<address> [length=1]", cmd_rotp, "Read Length (default 1) bytes of Data to OTP started at Address"	},
	{ "otp", "[dump|imp|exp|test|rpid|wpid|rvid|wvid|rba|wba|hid|cpw|pwridx|ledo] [file]; opt wba <BdAddr>",
		cmd_otp, "Misc OTP operation: dump/import otp content; imp file content into otp; test otp; otp wba <BdAddr>"	},
	{ "plb", "[1|0]", cmd_plb, "Enable/disable PCM CODEC loopback"	},
	{ "psw", "[1|0] [Frequency]", cmd_psw, "Enable/disable PCM sine wave playback at frequency (0..3700)"	},
	{ "lert", "<rx_channel>", cmd_lert, "Put unit in LE RX mode at rx_channel (0..39)"	},
	{ "lett", "<tx_channel> <length> <packet_payload>", cmd_lett, "Put unit in LE TX mode at tx_channel (0..39) with packet of given length (0..37) and packet_payload"	},
	{ "lete", "        ", cmd_lete, "End LE test"	},
	{ "tput-s", "[BD_Addr] [Judgment value] Logfile times data_size", cmd_tputs, "Throughput test - sender side"	},
	{ "tput-r", "        ", cmd_tputr, "Throughput test - receiver side"	},
	{ "btagent","<port number>", cmd_btagent, "BT Agent for IQFact" },
	{ "pinconntest", "        ", cmd_pinconntest, "Pin Connectivity Test"	},
	{ "hciinq", "        ", cmd_hciinq, "Inquiry start"	},
	{ "hciinqcnl", "        ", cmd_hciinqcnl, "Inquiry Cancel"	},
	{ "hcisetevtflt", "        ", cmd_hcisetevtflt, "Set Event Filter"	},
	{ "conn", "        ", cmd_createconnection, "ACL Connection Test" },
	{ "venspeccmd", "  ", cmd_venspeccmd, "Vendor Specific Command"	},
	{ "disc", "  ", cmd_disc, "HCI disconnect Command"	},
	{ "hciinvcmd1", "        ", cmd_hciinvcmd1, "Invalid HCI Command"	},
	{ "rawcmd", "    ", cmd_rawcmd, "RAW HCI Command ex) rawcmd ogf ocf <bytes>"	},
	{ NULL, NULL, NULL, NULL }
};
/*
	{ "get_id",   cmd_gid,    "Get Chip Identification Number"                },
*/
static void usage(void)
{
	int i;

	printf("btconfig - BTCONFIG Tool ver %s\n", VERSION);
	printf("Usage:\n"
		"\tbtconfig [options] <tty> <speed> <command> [command parameters]\n");
	printf("Options:\n"
		"\t--help\tDisplay help\n");
	printf("tty:\n"
		"\t/dev/ttyHS1\n");
	printf("Commands:\n");
	for (i = 0; command[i].cmd; i++)
		printf("\t%-8s %-40s\t%s\n", command[i].cmd,command[i].cmd_option,command[i].doc);
	printf("\n"
		"For more information on the usage of each command use:\n"
		"\tbtconfig <command> --help\n" );
}

struct ps_cfg_entry {
	uint32_t id;
	uint32_t len;
	uint8_t *data;
};

struct ps_entry_type {
	unsigned char type;
	unsigned char array;
};

struct ps_cfg_entry ps_list[MAX_TAGS];

static void load_hci_ps_hdr(uint8_t *cmd, uint8_t ps_op, int len, int index)
{
	hci_command_hdr *ch = (void *)cmd;

	ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
						HCI_PS_CMD_OCF));
	ch->plen = len + PS_HDR_LEN;
	cmd += HCI_COMMAND_HEADER_SIZE;

	cmd[0] = ps_op;
	cmd[1] = index;
	cmd[2] = index >> 8;
	cmd[3] = len;
}

/*
 * Send HCI command and wait for command complete event.
 * The event buffer has to be freed by the caller.
 */
static int send_hci_cmd_sync(int dev, uint8_t *cmd, int len, uint8_t **event)
{
	int err;
	uint8_t *hci_event;
	uint8_t pkt_type = 0x01; // HCI_COMMAND_PKT;

	if (len == 0)
		return len;

	if (write(dev, &pkt_type, 1) != 1)
		return -EILSEQ;
	if (write(dev, (unsigned char *)cmd, len) != len)
		return -EILSEQ;

	hci_event = (uint8_t *)malloc(PS_EVENT_LEN);
	if (!hci_event)
		return -ENOMEM;

	err = read_hci_event(dev, (unsigned char *)hci_event, PS_EVENT_LEN);
	if (err > 0) {
		*event = hci_event;
	} else {
		free(hci_event);
		return -EILSEQ;
	}

	return len;
}

static int read_ps_event(uint8_t *event, uint16_t ocf)
{
	hci_event_hdr *eh;
	uint16_t opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF, ocf));

	event++;

	eh = (void *)event;
	event += HCI_EVENT_HEADER_SIZE;

	// EVT_CMD_COMPLETE 0x0E
	if (eh->evt == 0x0E) {
		evt_cmd_complete *cc = (void *)event;

		// EVT_CMD_COMPLETE_SIZE 3
		event += 3;

		if (cc->opcode == opcode && event[0] == HCI_EV_SUCCESS)
			return 0;
		else
			return -EILSEQ;
	}

	return -EILSEQ;
}

static int write_cmd(int fd, uint8_t *buffer, int len)
{
	uint8_t *event;
	int err;

	err = send_hci_cmd_sync(fd, buffer, len, &event);
	if (err < 0)
		return err;

	err = read_ps_event(event, HCI_PS_CMD_OCF);

	free(event);

	return err;
}

/* Sends PS commands using vendor specficic HCI commands */
static int write_ps_cmd(int fd, uint8_t opcode, uint32_t ps_param)
{
	uint8_t cmd[HCI_MAX_CMD_SIZE];
	uint32_t i;

	switch (opcode) {
	case ENABLE_PATCH:
		load_hci_ps_hdr(cmd, opcode, 0, 0x00);

		if (write_cmd(fd, cmd, HCI_PS_CMD_HDR_LEN) < 0)
			return -EILSEQ;
		break;

	case PS_RESET:
		load_hci_ps_hdr(cmd, opcode, PS_RESET_PARAM_LEN, 0x00);

		cmd[7] = 0x00;
		cmd[PS_RESET_CMD_LEN - 2] = ps_param & PS_ID_MASK;
		cmd[PS_RESET_CMD_LEN - 1] = (ps_param >> 8) & PS_ID_MASK;

		if (write_cmd(fd, cmd, PS_RESET_CMD_LEN) < 0)
			return -EILSEQ;
		break;

	case PS_WRITE:
		for (i = 0; i < ps_param; i++) {
			load_hci_ps_hdr(cmd, opcode, ps_list[i].len,
							ps_list[i].id);

			memcpy(&cmd[HCI_PS_CMD_HDR_LEN], ps_list[i].data,
							ps_list[i].len);

			if (write_cmd(fd, cmd, ps_list[i].len +
						HCI_PS_CMD_HDR_LEN) < 0)
				return -EILSEQ;
		}
		break;
	}

	return 0;
}

#define __is_delim(ch) ((ch) == ':')
#define MAX_PREAMBLE_LEN 4

/* Parse PS entry preamble of format [X:X] for main type and subtype */
static int get_ps_type(char *ptr, int index, char *type, char *sub_type)
{
	int i;
	int delim = FALSE;

	if (index > MAX_PREAMBLE_LEN)
		return -EILSEQ;

	for (i = 1; i < index; i++) {
		if (__is_delim(ptr[i])) {
			delim = TRUE;
			continue;
		}

		if (isalpha(ptr[i])) {
			if (delim == FALSE)
				(*type) = toupper(ptr[i]);
			else
				(*sub_type) = toupper(ptr[i]);
		}
	}

	return 0;
}

#define ARRAY   'A'
#define STRING  'S'
#define DECIMAL 'D'
#define BINARY  'B'

#define PS_HEX           0
#define PS_DEC           1

static int get_input_format(char *buf, struct ps_entry_type *format)
{
	char *ptr = NULL;
	char type = '\0';
	char sub_type = '\0';

	format->type = PS_HEX;
	format->array = TRUE;

	if (strstr(buf, "[") != buf)
		return 0;

	ptr = strstr(buf, "]");
	if (!ptr)
		return -EILSEQ;

	if (get_ps_type(buf, ptr - buf, &type, &sub_type) < 0)
		return -EILSEQ;

	/* Check is data type is of array */
	if (type == ARRAY || sub_type == ARRAY)
		format->array = TRUE;

	if (type == STRING || sub_type == STRING)
		format->array = FALSE;

	if (type == DECIMAL || type == BINARY)
		format->type = PS_DEC;
	else
		format->type = PS_HEX;

	return 0;
}

#define UNDEFINED 0xFFFF

static unsigned int read_data_in_section(char *buf, struct ps_entry_type type)
{
	char *ptr = buf;

	if (!buf)
		return UNDEFINED;

	if (buf == strstr(buf, "[")) {
		ptr = strstr(buf, "]");
		if (!ptr)
			return UNDEFINED;

		ptr++;
	}

	if (type.type == PS_HEX && type.array != TRUE)
		return strtol(ptr, NULL, 16);

	return UNDEFINED;
}

struct tag_info {
	unsigned section;
	unsigned line_count;
	unsigned char_cnt;
	unsigned byte_count;
};

static inline int update_char_count(const char *buf)
{
	char *end_ptr;

	if (strstr(buf, "[") == buf) {
		end_ptr = strstr(buf, "]");
		if (!end_ptr)
			return 0;
		else
			return (end_ptr - buf) + 1;
	}

	return 0;
}

/* Read PS entries as string, convert and add to Hex array */
static void update_tag_data(struct ps_cfg_entry *tag,
				struct tag_info *info, const char *ptr)
{
	char buf[3];

	buf[2] = '\0';

	strlcpy(buf, &ptr[info->char_cnt], 2);
	tag->data[info->byte_count] = strtol(buf, NULL, 16);
	info->char_cnt += 3;
	info->byte_count++;

	strlcpy(buf, &ptr[info->char_cnt], 2);
	tag->data[info->byte_count] = strtol(buf, NULL, 16);
	info->char_cnt += 3;
	info->byte_count++;
}

#define PS_UNDEF   0
#define PS_ID      1
#define PS_LEN     2
#define PS_DATA    3

#define PS_MAX_LEN         500
#define ENTRY_PER_LINE     16

#define __check_comment(buf) (((buf)[0] == '/') && ((buf)[1] == '/'))
#define __skip_space(str)      while (*(str) == ' ') ((str)++)

static int ath_parse_ps(FILE *stream)
{
	char buf[LINE_SIZE_MAX + 1];
	char *ptr;
	uint8_t tag_cnt = 0;
	uint8_t tagPlatformConfig = 0;
	int16_t byte_count = 0;
	struct ps_entry_type format;
	struct tag_info status = { 0, 0, 0, 0 };

	do {
		int read_count;
		struct ps_cfg_entry *tag;

		ptr = fgets(buf, LINE_SIZE_MAX, stream);
		if (!ptr)
			break;

		__skip_space(ptr);
		if (__check_comment(ptr))
			continue;

		/* Lines with a '#' will be followed by new PS entry */
		if (ptr == strstr(ptr, "#")) {
			if (status.section != PS_UNDEF) {
				return -EILSEQ;
			} else {
				status.section = PS_ID;
				continue;
			}
		}

		tag = &ps_list[tag_cnt];

		switch (status.section) {
		case PS_ID:
			if (get_input_format(ptr, &format) < 0)
				return -EILSEQ;

			tag->id = read_data_in_section(ptr, format);
			if (tag->id == 0x21) /* Platform Config */
				tagPlatformConfig = 1;
			status.section = PS_LEN;
			break;

		case PS_LEN:
			if (get_input_format(ptr, &format) < 0)
				return -EILSEQ;

			byte_count = read_data_in_section(ptr, format);
			if (byte_count > PS_MAX_LEN)
				return -EILSEQ;

			tag->len = byte_count;
			tag->data = (uint8_t *)malloc(byte_count);

			status.section = PS_DATA;
			status.line_count = 0;
			break;

		case PS_DATA:
			if (status.line_count == 0)
				if (get_input_format(ptr, &format) < 0)
					return -EILSEQ;

			__skip_space(ptr);

			status.char_cnt = update_char_count(ptr);

			read_count = (byte_count > ENTRY_PER_LINE) ?
					ENTRY_PER_LINE : byte_count;

			if (format.type == PS_HEX && format.array == TRUE) {
				while (read_count > 0) {
					update_tag_data(tag, &status, ptr);
					read_count -= 2;
				}

	                        if (tagPlatformConfig == 1) {
								tagPlatformConfig = 0;
								tag->data[0] &= 0x7f;
							}

				if (byte_count > ENTRY_PER_LINE)
					byte_count -= ENTRY_PER_LINE;
				else
					byte_count = 0;
			}

			status.line_count++;

			if (byte_count == 0)
				memset(&status, 0x00, sizeof(struct tag_info));

			if (status.section == PS_UNDEF)
				tag_cnt++;

			if (tag_cnt == MAX_TAGS)
				return -EILSEQ;
			break;
		}
	} while (ptr);

	return tag_cnt;
}

#define MAX_PATCH_CMD 244
struct patch_entry {
	int16_t len;
	uint8_t data[MAX_PATCH_CMD];
};

#define SET_PATCH_RAM_ID	0x0D
#define SET_PATCH_RAM_CMD_SIZE	11
#define ADDRESS_LEN		4
static int set_patch_ram(int dev, char *patch_loc, int len)
{
	int err;
	uint8_t cmd[20];
	int i, j;
	char loc_byte[3];
	uint8_t *event;
	uint8_t *loc_ptr = &cmd[7];

	if (!patch_loc)
		return -1;

	loc_byte[2] = '\0';

	load_hci_ps_hdr(cmd, SET_PATCH_RAM_ID, ADDRESS_LEN, 0);

	for (i = 0, j = 3; i < 4; i++, j--) {
		loc_byte[0] = patch_loc[0];
		loc_byte[1] = patch_loc[1];
		loc_ptr[j] = strtol(loc_byte, NULL, 16);
		patch_loc += 2;
	}

	err = send_hci_cmd_sync(dev, cmd, SET_PATCH_RAM_CMD_SIZE, &event);
	if (err < 0)
		return err;

	err = read_ps_event(event, HCI_PS_CMD_OCF);

	free(event);

	return err;
}

#define PATCH_LOC_KEY    "DA:"
#define PATCH_LOC_STRING_LEN    8
static int ps_patch_download(int fd, FILE *stream)
{
	char byte[3];
	char ptr[MAX_PATCH_CMD + 1];
	int byte_cnt;
	int patch_count = 0;
	char patch_loc[PATCH_LOC_STRING_LEN + 1];

	byte[2] = '\0';

	while (fgets(ptr, MAX_PATCH_CMD, stream)) {
		if (strlen(ptr) <= 1)
			continue;
		else if (strstr(ptr, PATCH_LOC_KEY) == ptr) {
			strlcpy(patch_loc, &ptr[sizeof(PATCH_LOC_KEY) - 1],
							PATCH_LOC_STRING_LEN);
			if (set_patch_ram(fd, patch_loc, sizeof(patch_loc)) < 0)
				return -1;
		} else if (isxdigit(ptr[0]))
			break;
		else
			return -1;
	}

	byte_cnt = strtol(ptr, NULL, 16);

	while (byte_cnt > 0) {
		int i;
		uint8_t cmd[HCI_MAX_CMD_SIZE];
		struct patch_entry patch;

		if (byte_cnt > MAX_PATCH_CMD)
			patch.len = MAX_PATCH_CMD;
		else
			patch.len = byte_cnt;

		for (i = 0; i < patch.len; i++) {
			if (!fgets(byte, 3, stream))
				return -1;

			patch.data[i] = strtoul(byte, NULL, 16);
		}

		load_hci_ps_hdr(cmd, WRITE_PATCH, patch.len, patch_count);
		memcpy(&cmd[HCI_PS_CMD_HDR_LEN], patch.data, patch.len);

		if (write_cmd(fd, cmd, patch.len + HCI_PS_CMD_HDR_LEN) < 0)
			return -1;

		patch_count++;
		byte_cnt = byte_cnt - MAX_PATCH_CMD;
	}

	if (write_ps_cmd(fd, ENABLE_PATCH, 0) < 0)
		return -1;

	return patch_count;
}

static int ps_config_download(int fd, int tag_count)
{
	if (write_ps_cmd(fd, PS_RESET, PS_RAM_SIZE) < 0)
		return -1;

	if (tag_count > 0)
		if (write_ps_cmd(fd, PS_WRITE, tag_count) < 0)
			return -1;
	return 0;
}

static void get_ps_file_name(uint32_t devtype, uint32_t rom_version,
							char *path)
{
	char *filename;

	if (devtype == ROM_DEV_TYPE)
		filename = PS_ASIC_FILE;
	else
		filename = PS_FPGA_FILE;

	snprintf(path, MAXPATHLEN, "%s%x/%s", FW_PATH, rom_version, filename);
}

static void get_patch_file_name(uint32_t dev_type, uint32_t rom_version,
				uint32_t build_version, char *path)
{
	if (rom_version == FPGA_ROM_VERSION && dev_type != ROM_DEV_TYPE &&
					dev_type != 0 && build_version == 1)
		path[0] = '\0';
	else
		snprintf(path, MAXPATHLEN, "%s%x/%s",
				FW_PATH, rom_version, PATCH_FILE);
}

#define VERIFY_CRC   9
#define PS_REGION    1
#define PATCH_REGION 2

static int get_ath3k_crc(int dev)
{
	uint8_t cmd[7];
	uint8_t *event;
	int err;

	load_hci_ps_hdr(cmd, VERIFY_CRC, 0, PS_REGION | PATCH_REGION);

	err = send_hci_cmd_sync(dev, cmd, sizeof(cmd), &event);
	if (err < 0)
		return err;
	/* Send error code if CRC check patched */
	if (read_ps_event(event, HCI_PS_CMD_OCF) >= 0)
		err = -EILSEQ;

	free(event);

	return err;
}

#define DEV_REGISTER      0x4FFC
#define GET_DEV_TYPE_OCF  0x05

static int get_device_type(int dev, uint32_t *code)
{
	uint8_t cmd[8];
	uint8_t *event;
	uint32_t reg;
	int err;
	uint8_t *ptr = cmd;
	hci_command_hdr *ch = (void *)cmd;

	ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
						GET_DEV_TYPE_OCF));
	ch->plen = 5;
	ptr += HCI_COMMAND_HEADER_SIZE;

	ptr[0] = (uint8_t)DEV_REGISTER;
	ptr[1] = (uint8_t)DEV_REGISTER >> 8;
	ptr[2] = (uint8_t)DEV_REGISTER >> 16;
	ptr[3] = (uint8_t)DEV_REGISTER >> 24;
	ptr[4] = 0x04;

	err = send_hci_cmd_sync(dev, cmd, sizeof(cmd), &event);
	if (err < 0)
		return err;

	err = read_ps_event(event, GET_DEV_TYPE_OCF);
	if (err < 0)
		goto cleanup;

	reg = event[10];
	reg = (reg << 8) | event[9];
	reg = (reg << 8) | event[8];
	reg = (reg << 8) | event[7];
	*code = reg;

cleanup:
	free(event);

	return err;
}

static int read_ath3k_version(int pConfig, uint32_t *rom_version,
					uint32_t *build_version)
{
	uint8_t cmd[3];
	uint8_t *event;
	int err;
	int status;
	hci_command_hdr *ch = (void *)cmd;

	ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
						OCF_READ_VERSION));
	ch->plen = 0;

	err = send_hci_cmd_sync(pConfig, cmd, sizeof(cmd), &event);
	if (err < 0)
		return err;

	err = read_ps_event(event, OCF_READ_VERSION);
	if (err < 0)
		goto cleanup;

	status = event[10];
	status = (status << 8) | event[9];
	status = (status << 8) | event[8];
	status = (status << 8) | event[7];
	*rom_version = status;

	status = event[14];
	status = (status << 8) | event[13];
	status = (status << 8) | event[12];
	status = (status << 8) | event[11];
	*build_version = status;

cleanup:
	free(event);

	return err;
}

static void convert_bdaddr(char *str_bdaddr, char *bdaddr)
{
	char bdbyte[3];
	char *str_byte = str_bdaddr;
	int i, j;
	int colon_present = 0;

	if (strstr(str_bdaddr, ":"))
		colon_present = 1;

	bdbyte[2] = '\0';

	/* Reverse the BDADDR to LSB first */
	for (i = 0, j = 5; i < 6; i++, j--) {
		bdbyte[0] = str_byte[0];
		bdbyte[1] = str_byte[1];
		bdaddr[j] = strtol(bdbyte, NULL, 16);

		if (colon_present == 1)
			str_byte += 3;
		else
			str_byte += 2;
	}
}

static int write_bdaddr(int pConfig, char *bdaddr)
{
	uint8_t *event;
	int err;
	uint8_t cmd[13];
	uint8_t *ptr = cmd;
	hci_command_hdr *ch = (void *)cmd;

	memset(cmd, 0, sizeof(cmd));

	ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
						HCI_PS_CMD_OCF));
	ch->plen = 10;
	ptr += HCI_COMMAND_HEADER_SIZE;

	ptr[0] = 0x01;
	ptr[1] = 0x01;
	ptr[2] = 0x00;
	ptr[3] = 0x06;

	convert_bdaddr(bdaddr, (char *)&ptr[4]);

	err = send_hci_cmd_sync(pConfig, cmd, sizeof(cmd), &event);
	if (err < 0)
		return err;

	err = read_ps_event(event, HCI_PS_CMD_OCF);

	free(event);

	return err;
}

static void write_bdaddr_from_file(int rom_version, int fd)
{
	FILE *stream;
	char bdaddr[PATH_MAX];
	char bdaddr_file[PATH_MAX];

	snprintf(bdaddr_file, MAXPATHLEN, "%s%x/%s",
			FW_PATH, rom_version, BDADDR_FILE);

	stream = fopen(bdaddr_file, "r");
	if (!stream)
		return;

	if (fgets(bdaddr, PATH_MAX - 1, stream))
		write_bdaddr(fd, bdaddr);

	fclose(stream);
}

static int ath_ps_download(int fd)
{
	int err = 0;
	int tag_count = 0;
	int patch_count = 0;
	uint32_t rom_version = 0;
	uint32_t build_version = 0;
	uint32_t dev_type = 0;
	char patch_file[PATH_MAX];
	char ps_file[PATH_MAX];
	FILE *stream;

	/*
	 * Verfiy firmware version. depending on it select the PS
	 * config file to download.
	 */
	if (get_device_type(fd, &dev_type) < 0) {
		err = -EILSEQ;
		goto download_cmplete;
	}

	if (read_ath3k_version(fd, &rom_version, &build_version) < 0) {
		err = -EILSEQ;
		goto download_cmplete;
	}

	/* Do not download configuration if CRC passes */
	if (get_ath3k_crc(fd) < 0) {
		err = 0;
		goto download_cmplete;
	}

	get_ps_file_name(dev_type, rom_version, ps_file);
	get_patch_file_name(dev_type, rom_version, build_version, patch_file);

	stream = fopen(ps_file, "r");
	if (!stream) {
		printf("firmware file open error:%s, ver:%x\n",ps_file, rom_version);
		if (rom_version == 0x1020201)
			err = 0;
		else
			err = -EILSEQ;
		goto download_cmplete;
	}
	else {
		tag_count = ath_parse_ps(stream);
		fclose(stream);
	}

	if (tag_count < 0) {
		err = -EILSEQ;
		goto download_cmplete;
	}

	/*
	 * It is not necessary that Patch file be available,
	 * continue with PS Operations if patch file is not available.
	 */
	if (patch_file[0] == '\0')
		err = 0;

	stream = fopen(patch_file, "r");
	if (!stream)
		err = 0;
	else {
		patch_count = ps_patch_download(fd, stream);
		fclose(stream);

		if (patch_count < 0) {
			err = -EILSEQ;
			goto download_cmplete;
		}
	}

	err = ps_config_download(fd, tag_count);

download_cmplete:
	if (!err)
		write_bdaddr_from_file(rom_version, fd);

	return err;
}

static int set_cntrlr_baud(int fd, int speed)
{
	int baud;
	struct timespec tm = { 0, 500000 };
	unsigned char cmd[MAX_CMD_LEN], rsp[MAX_EVENT_SIZE];
	unsigned char *ptr = cmd + 1;
	hci_command_hdr *ch = (void *)ptr;

	cmd[0] = 0x01; //HCI_COMMAND_PKT;

	/* set controller baud rate to user specified value */
	ptr = cmd + 1;
	ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
						HCI_CHG_BAUD_CMD_OCF));
	ch->plen = 2;
	ptr += HCI_COMMAND_HEADER_SIZE;

	baud = speed/100;
	ptr[0] = (char)baud;
	ptr[1] = (char)(baud >> 8);

	if (write(fd, cmd, WRITE_BAUD_CMD_LEN) != WRITE_BAUD_CMD_LEN) {
		perror("Failed to write change baud rate command");
		return -ETIMEDOUT;
	}

	nanosleep(&tm, NULL);

	if (read_hci_event(fd, rsp, sizeof(rsp)) < 0)
		return -ETIMEDOUT;

	return 0;
}

static int uart_speed(int s)
{
	switch (s) {
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	case 500000:
		return B500000;
	case 576000:
		return B576000;
	case 921600:
		return B921600;
	case 1000000:
		return B1000000;
	case 1152000:
		return B1152000;
	case 1500000:
		return B1500000;
	case 2000000:
		return B2000000;
#ifdef B2500000
	case 2500000:
		return B2500000;
#endif
	case 3000000:
		return B3000000;
#ifdef B3500000
	case 3500000:
		return B3500000;
#endif
#ifdef B4000000
	case 4000000:
		return B4000000;
#endif
	default:
		return B57600;
	}
}

int set_speed(int fd, struct termios *ti, int speed)
{
	if (cfsetospeed(ti, uart_speed(speed)) < 0)
		return -errno;

	if (cfsetispeed(ti, uart_speed(speed)) < 0)
		return -errno;

	if (tcsetattr(fd, TCSANOW, ti) < 0)
		return -errno;

	return 0;
}


/*
 * Atheros AR300x specific initialization and configuration file
 * download
 */
int ath3k_init(int fd, int speed, int init_speed, char *bdaddr, struct termios *ti)
{
	int r;
	int err = 0;
	struct timespec tm = { 0, 500000 };
	unsigned char cmd[MAX_CMD_LEN], rsp[MAX_EVENT_SIZE];
	unsigned char *ptr = cmd + 1;
	hci_command_hdr *ch = (void *)ptr;

	cmd[0] = 0x01; //HCI_COMMAND_PKT;

	/* set both controller and host baud rate to maximum possible value */
	err = set_cntrlr_baud(fd, speed);
	if (err < 0)
		return err;

	err = set_speed(fd, ti, speed);
	if (err < 0) {
		perror("Can't set required baud rate");
		return err;
	}

	/* Download PS and patch */
	r = ath_ps_download(fd);
	if (r < 0) {
		perror("Failed to Download configuration");
		err = -ETIMEDOUT;
		goto failed;
	}

	/* Write BDADDR */
	if (bdaddr) {
		ch->opcode = htobs(HCI_OPCODE_PACK(HCI_VENDOR_CMD_OGF,
							HCI_PS_CMD_OCF));
		ch->plen = 10;
		ptr += HCI_COMMAND_HEADER_SIZE;

		ptr[0] = 0x01;
		ptr[1] = 0x01;
		ptr[2] = 0x00;
		ptr[3] = 0x06;
		str2ba(bdaddr, (bdaddr_t *)(ptr + 4));

		if (write(fd, cmd, WRITE_BDADDR_CMD_LEN) !=
					WRITE_BDADDR_CMD_LEN) {
			perror("Failed to write BD_ADDR command\n");
			err = -ETIMEDOUT;
			goto failed;
		}

		if (read_hci_event(fd, rsp, sizeof(rsp)) < 0) {
			perror("Failed to set BD_ADDR\n");
			err = -ETIMEDOUT;
			goto failed;
		}
	}

	/* Send HCI Reset */
	cmd[1] = 0x03; // OCF_RESET
	cmd[2] = 0x0C;
	cmd[3] = 0x00;

	r = write(fd, cmd, 4);
	if (r != 4) {
		err = -ETIMEDOUT;
		goto failed;
	}

	nanosleep(&tm, NULL);
	if (read_hci_event(fd, rsp, sizeof(rsp)) < 0) {
		err = -ETIMEDOUT;
		goto failed;
	}

	err = set_cntrlr_baud(fd, speed);
	if (err < 0)
		return err;

failed:
	if (err < 0) {
		set_cntrlr_baud(fd, init_speed);
		set_speed(fd, ti, init_speed);
	}

	return err;
}

/* Initialize SMD driver */
static int init_smd(char *dev)
{
	int retry = 0;
	struct termios term;
	int fd = -1;

	fd = open(dev, (O_RDWR | O_NOCTTY));

	while ((-1 == fd) && (retry < 3)) {
		perror("Cannot open device. Retry after 2 seconds");
		usleep(2000000);
		fd = open(dev, (O_RDWR | O_NOCTTY));
		retry++;
	}

	if (-1 == fd) {
		perror("Cannot open device, will exit");
		return -1;
	}

	usleep(500000);

	if (tcflush(fd, TCIOFLUSH) < 0) {
		perror("Cannot flush device");
		close(fd);
		return -1;
	}

	if (tcgetattr(fd, &term) < 0) {
		perror("Error while getting attributes");
		close(fd);
		return -1;
	}

	cfmakeraw(&term);
	term.c_cflag |= (CRTSCTS | CLOCAL);

	if (tcsetattr(fd, TCSANOW, &term) < 0) {
		perror("Error while getting attributes");
		close(fd);
		return -1;
	}

	printf("Done intiailizing fd = %s \n", dev);
	return fd;
}

/* Initialize UART driver */
static int init_uart(char *dev, int user_specified_speed)
{
	struct termios ti;
	int fd;
	unsigned long flags = 0;


	fd = open(dev, O_RDWR | O_NOCTTY);
	if (fd < 0) {
		perror("Can't open serial port");
		return -1;
	}

	tcflush(fd, TCIOFLUSH);

	if (tcgetattr(fd, &ti) < 0) {
		perror("Can't get port settings");
		return -1;
	}

	cfmakeraw(&ti);

	ti.c_cflag |= CLOCAL;
	ti.c_cflag |= CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &ti) < 0) {
		perror("Can't set port settings");
		close(fd);
		return -1;
	}

	/* Set initial baudrate */
	if (set_speed(fd, &ti, 115200) < 0) {
		perror("Can't set initial baud rate");
		close(fd);
		return -1;
	}

	tcflush(fd, TCIOFLUSH);


	if (ioctl(fd, TIOCMGET, &flags) < 0) {
                perror("TIOCMGET failed in init\n");
		close(fd);
                return -1;
        }

        flags |= TIOCM_RTS;
        if (ioctl(fd, TIOCMSET, &flags) < 0) {
                perror("TIOCMSET failed in init: HW Flow-on error\n");
				close(fd);
				return -1;
        }

	if (ath3k_init(fd, user_specified_speed,115200, NULL, &ti) < 0){
		close(fd);
		return -1;
	}

	tcflush(fd, TCIOFLUSH);

	return fd;
}

int main(int argc, char *argv[])
{
	int opt, i, ret;
	int fd = -1;
	char prop[30];


	while ((opt=getopt_long(argc, argv, "+i:h", main_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
		default:
			usage();
			exit(0);
		}
	}

	argc -= optind;
	argv += optind;
	optind = 0;

	if (argc < 2) {
		usage();
		exit(0);
	}
	
	ret = property_get("ro.qualcomm.bt.hci_transport", prop, NULL);

	if (!strcasecmp(prop, "smd")) {
		fd = init_smd(argv[optind]);
	} else if ((fd = init_uart(argv[optind], atoi(argv[optind+1]))) < 0) {
		perror("Device is not available");
		exit(1);
	}

	argv +=1;
	argc -=1;

	for (i = 0; command[i].cmd; i++) {
		if (strcmp(command[i].cmd, argv[0]))
			continue;
		command[i].func(fd, argc, argv);
		break;
	}
	close(fd);
	return 0;
}


// MAster BLaster fucntions
tMasterBlasterField MasterBlasterMenu[] =
{
   {"ContRxMode", "n", "toggle coNtinuous Rx", 0,                     ContRxModeOption,   SetMasterBlasterContRxMode},
   {"ContTxMode", "c", "toggle Continuous Tx", 0,                     ContTxModeOption,   SetMasterBlasterContTxMode},
   {"LERxMode",   "q", "toggle LE Rx mode",    0,                     ContRxModeOption,   SetMasterBlasterLERxMode},
   {"LETxMode",   "w", "toggle LE Tx mode",    0,                     ContTxModeOption,   SetMasterBlasterLETxMode},
   {"LETxPktPayload", "y", "set LE Tx packet payload", 0,             LETxPktPayloadOption, SetMasterBlasterLETxPktPayload},
   {"ContTxType", "u", "toggle continUous Tx Type", Cont_Tx_Raw_1MHz, ContTxTypeOption,   SetMasterBlasterContTxType},
   {"TestMode",   "m", "toggle test Mode",     eBRM_TestMode_TX_1010, TestModeOption,     SetMasterBlasterTestMode},
   {"HopMode",    "h", "toggle Hop mode",      0,                     HopModeOption,      SetMasterBlasterHopMode},
   {"TxFreq",     "t", "set Tx freq",          39,                    NULL,               SetMasterBlasterTxFreq},
   {"RxFreq",     "r", "set Rx freq",          39,                    NULL,               SetMasterBlasterRxFreq},
   {"PacketType", "p", "toggle Packet type",   TxTest_PktType_DH1,    PacketTypeOption,   SetMasterBlasterPacketType},
   {"DataLen",    "l", "set data Length",      15,                    NULL,               SetMasterBlasterDataLen},
   {"Power",      "o", "toggle pOwer",         9,                     NULL,               SetMasterBlasterPower},
   {"BdAddr",     "a", "set bdAddr",           0,                     NULL,               SetMasterBlasterBdAddr},
   {"SetBERType", "k", "set BER type",         eBRM_BERMode_ALL_DATA, BERPacketTypeOption,SetMasterBlasterBERType},
   {"GetBER",     "g", "get BER type",	       0,		      NULL,		  SetMasterBlasterNothing},
   {"EnableRxTest", "d", "Enable rx test mode", 0,                    NULL,               SetMasterBlasterNothing},
   {"EnableTxTest", "e", "Enable tx test mode", 0,                    NULL,               SetMasterBlasterNothing},
   {"EnableTest", "j", "Start test mode",      0,                     NULL,               SetMasterBlasterNothing},
   {"StopTest",   "s", "Stop test mode",       0,                     NULL,               SetMasterBlasterNothing},
   {"Exit",       "x", "eXit",                 0,                     NULL,               SetMasterBlasterNothing},
   {"ExitWithoutReset", "b", "Exit without reset", 0,                 NULL,               SetMasterBlasterNothing},
};

tPsSysCfgTransmitPowerControlTable  TpcTable;

//----------------------------------------------------------------------------

void InitMasterBlaster (tBRM_Control_packet *MasterBlaster, bdaddr_t *BdAddr, UCHAR *SkipRxSlot)
{
   *SkipRxSlot = 0x01;
   MasterBlaster->testCtrl.Mode     = MasterBlasterMenu[TM].Default;
   MasterBlaster->testCtrl.HopMode  = MasterBlasterMenu[HM].Default;
   MasterBlaster->testCtrl.Packet   = MasterBlasterMenu[PT].Default;
   MasterBlaster->testCtrl.TxFreq   = MasterBlasterMenu[TF].Default;
   MasterBlaster->testCtrl.RxFreq   = MasterBlasterMenu[RF].Default;
   MasterBlaster->testCtrl.Power    = MasterBlasterMenu[PO].Default;
   MasterBlaster->testCtrl.DataLen  = MasterBlasterMenu[DL].Default;
   MasterBlaster->ContTxMode        = MasterBlasterMenu[CT].Default;
   MasterBlaster->ContTxType        = MasterBlasterMenu[CX].Default;
   MasterBlaster->ContRxMode        = MasterBlasterMenu[CR].Default;
   MasterBlaster->BERType           = MasterBlasterMenu[SB].Default;
   MasterBlaster->LERxMode          = MasterBlasterMenu[LR].Default;
   MasterBlaster->LETxMode          = MasterBlasterMenu[LT].Default;
   MasterBlaster->LETxParms.PktPayload = MasterBlasterMenu[LTM].Default;
   memcpy(MasterBlaster->bdaddr,&BdAddr->b[0],6);

   TpcTable.NumOfEntries = 0;
}

//----------------------------------------------------------------------------

int CheckField (tBRM_Control_packet MasterBlaster, char *FieldAlias)
{
   if (((!strncmp(FieldAlias,MasterBlasterMenu[HM].Alias,1)) && MasterBlaster.ContTxMode) ||
       (((!strncmp(FieldAlias,MasterBlasterMenu[TF].Alias,1)) || (!strncmp(FieldAlias,MasterBlasterMenu[RF].Alias,1))) && MasterBlaster.testCtrl.HopMode == 1) ||
       ((!strncmp(FieldAlias,MasterBlasterMenu[CX].Alias,1)) && MasterBlaster.ContTxMode == 0))
   {
	 return INVALID_MASTERBLASTER_FIELD;
   }
   unsigned int i;
   for (i = 0; i < sizeof(MasterBlasterMenu)/sizeof(tMasterBlasterField); ++i)
   {
      if (!strncmp(FieldAlias,MasterBlasterMenu[i].Alias,1))
      {
         return i;
      }
   }

   return INVALID_MASTERBLASTER_FIELD;
}

//----------------------------------------------------------------------------

int GetTestModeOptionIndex (int Value)
{
   unsigned int i;
   for (i = 0; i < sizeof(TestModeOption)/sizeof(tMasterBlasterOption); ++i)
   {
      if (Value == TestModeOption[i].Value)
      {
         return i;
      }
   }
  // assert (0);
   return -1;
}

//----------------------------------------------------------------------------

int GetPacketTypeOptionIndex (int Value)
{
   unsigned int i;
   for (i = 0; i < sizeof(PacketTypeOption)/sizeof(tMasterBlasterOption); ++i)
   {
      if (Value == PacketTypeOption[i].Value)
      {
         return i;
      }
   }
   //assert (0);
   return -1;
}

//----------------------------------------------------------------------------

void PrintMasterBlasterMenu(tBRM_Control_packet *MasterBlaster)
{
	unsigned int i;
	printf ("\n---------- Master Blaster Mode ----------\n\n");
	for (i = 0; i < sizeof(MasterBlasterMenu)/sizeof(tMasterBlasterField); ++i)
	{
		if (((i == HM || i == RF) && (MasterBlaster->ContTxMode == ENABLE)) ||
			((i == TF || i == RF) && (MasterBlaster->testCtrl.HopMode == 1)) ||
			((i == CX) && (MasterBlaster->ContTxMode == DISABLE)) ||
			((i == CX || i == HM || i == TF || i == PT || i == DL || i == PO || i == BA) &&
			(MasterBlaster->ContRxMode == ENABLE)))
		{
			continue;
		}

		printf ("\t%s - %s\n", MasterBlasterMenu[i].Alias, MasterBlasterMenu[i].Usage);
	}
	printf ("\n-----------------------------------------\n\n");

	char BdAddr[18];
	//strcpy(MasterBlaster.bdaddr,BdAddr);

	printf ("ContRxMode: %s\n", ContRxModeOption[MasterBlaster->ContRxMode].Name);
	printf ("ContTxMode: %s\n", ContTxModeOption[MasterBlaster->ContTxMode].Name);
	printf ("LERxMode: %s\n", ContTxModeOption[MasterBlaster->LERxMode].Name);
	printf ("LETxMode: %s\n", ContTxModeOption[MasterBlaster->LETxMode].Name);

	// LE Rx mode
	if (MasterBlaster->LERxMode == ENABLE)
	{
		if (MasterBlaster->testCtrl.RxFreq > MB_MAX_FREQUENCY_LE)
			MasterBlaster->testCtrl.RxFreq = MB_MAX_FREQUENCY_LE;
		printf("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
	}
	// LE Tx mode
	if (MasterBlaster->LETxMode == ENABLE)
	{
		if (MasterBlaster->testCtrl.DataLen > MB_MAX_DATALEN_LE)
			MasterBlaster->testCtrl.DataLen = MB_MAX_DATALEN_LE;
		printf("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
		printf("DataLen:    %d\n", MasterBlaster->testCtrl.DataLen);
		printf("PktPayload: %s\n", LETxPktPayloadOption[MasterBlaster->LETxParms.PktPayload].Name);
	}
	// Continous Rx mode
	else if (MasterBlaster->ContRxMode == ENABLE)
	{
		printf ("BER Type: %s\n",BERPacketTypeOption[MasterBlaster->BERType].Name);
		printf ("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
	}
	// Continous Tx mode and Tx test mode
	else
	{
		printf ("BER Type: %s\n",BERPacketTypeOption[MasterBlaster->BERType].Name);
		if (MasterBlaster->ContTxMode == ENABLE)
		{
			printf ("ContTxType: %s\n", ContTxTypeOption[MasterBlaster->ContTxType].Name);
			if (ContTxTypeOption[MasterBlaster->ContTxType].Value != CW_Single_Tone)
				printf ("TestMode:   %s\n", TestModeOption[GetTestModeOptionIndex(MasterBlaster->testCtrl.Mode)].Name);
			printf ("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
		}
		else
		{
			printf ("TestMode:   %s\n", TestModeOption[GetTestModeOptionIndex(MasterBlaster->testCtrl.Mode)].Name);
			printf ("HopMode:    %s\n", HopModeOption[MasterBlaster->testCtrl.HopMode].Name);

			if (MasterBlaster->testCtrl.HopMode == 0)
			{
				printf ("TxFreq:     %d\n", MasterBlaster->testCtrl.TxFreq);
				printf ("RxFreq:     %d\n", MasterBlaster->testCtrl.RxFreq);
			}
		}
		if (TpcTable.NumOfEntries > 0)
		{
			printf ("Power:      Step = %d/%d; Level = %d dBm\n", MasterBlaster->testCtrl.Power+1,
			TpcTable.NumOfEntries, TpcTable.t[MasterBlaster->testCtrl.Power].TxPowerLevel);
		}
		else
		{
			printf ("Power:      Step = Max; Level = N/A\n");
		}
		if ((MasterBlaster->ContTxMode == ENABLE && ContTxTypeOption[MasterBlaster->ContTxType].Value == Cont_Tx_Regular) ||
		(MasterBlaster->ContTxMode == DISABLE))
		{
			printf ("PacketType: %s\n", PacketTypeOption[GetPacketTypeOptionIndex(MasterBlaster->testCtrl.Packet)].Name);
			printf ("DataLen:    %d\n", MasterBlaster->testCtrl.DataLen);
		}
		if (ContTxTypeOption[MasterBlaster->ContTxType].Value != CW_Single_Tone) {//for single tone, no bdaddr
			ba2str((const bdaddr_t *)MasterBlaster->bdaddr, BdAddr);
			printf ("BdAddr:     0x%s\n\n",BdAddr);
		}
	}
	printf ("\nmb>\n");
}

//----------------------------------------------------------------------------

int SetMasterBlasterTestMode(tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
	int Value = (int)MasterBlaster->testCtrl.Mode;

	if (ToggleOption (&Value, Option, TestModeOption,
			  sizeof(TestModeOption)/sizeof(tMasterBlasterOption), TM,1))
	{
		MasterBlaster->testCtrl.Mode = (UCHAR)Value;
		// Enable continous Tx should disable continous Rx
		MasterBlaster->ContRxMode = DISABLE;
		MasterBlaster->ContTxMode = DISABLE;
		return TRUE;
	}
	return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterHopMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->testCtrl.HopMode;

   if (ToggleOption (&Value, Option, HopModeOption,
                        sizeof(HopModeOption)/sizeof(tMasterBlasterOption), HM,1))
   {
      MasterBlaster->testCtrl.HopMode = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterTxFreq (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   //char Buffer[20];
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int Value = (int)MasterBlaster->testCtrl.TxFreq;
   int MaxFreq = LEMode ? MB_MAX_FREQUENCY_LE : MB_MAX_FREQUENCY;
   int MinFreq = LEMode ? MB_MIN_FREQUENCY_LE : MB_MIN_FREQUENCY;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter Tx frequency (%d..%d): ", MinFreq, MaxFreq);
      scanf("%d",&NewValue.Value);
//    fgets(NewValue,3,stdin);
      if (MinMaxOption (&Value, &NewValue, MinFreq, MaxFreq))
      {
         MasterBlaster->testCtrl.TxFreq = (UCHAR)Value;
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid Tx frequency.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterRxFreq (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int Value = (int)MasterBlaster->testCtrl.RxFreq;
   int MaxFreq = LEMode ? MB_MAX_FREQUENCY_LE : MB_MAX_FREQUENCY;
   int MinFreq = LEMode ? MB_MIN_FREQUENCY_LE : MB_MIN_FREQUENCY;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter Rx frequency (%d..%d): ", MinFreq, MaxFreq);
      scanf("%d",&NewValue.Value);
      if (MinMaxOption (&Value, &NewValue, MinFreq, MaxFreq))
      {
         MasterBlaster->testCtrl.RxFreq = (UCHAR)Value;
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid Rx frequency.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterPacketType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->testCtrl.Packet;

   if (ToggleOption (&Value, Option, PacketTypeOption,
                        sizeof(PacketTypeOption)/sizeof(tMasterBlasterOption), PT,1))
   {
      MasterBlaster->testCtrl.Packet = (UCHAR)Value;
      MasterBlaster->testCtrl.DataLen = MaxDataLenOption[GetPacketTypeOptionIndex(Value)];
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterDataLen (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   tMasterBlasterOption NewValue;
   int LoopCount = 4;
   int MaxLen = LEMode ? MB_MAX_DATALEN_LE : MB_MAX_DATALEN;
   int MinLen = LEMode ? MB_MIN_DATALEN_LE : MB_MIN_DATALEN;

   while (--LoopCount > 0)
   {
      printf ("\n   Enter data length (%d..%d): ", MinLen, MaxLen);
      scanf("%d",&NewValue.Value);
      if (MinMaxOption (&MasterBlaster->testCtrl.DataLen, &NewValue, MinLen, MaxLen))
      {
         return TRUE;
      }
      else if (LoopCount > 1)
      {
         printf ("\n   ERROR ---> Invalid data length.\n");
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterPower (tBRM_Control_packet *MasterBlaster, char *Option)
{
   if (TpcTable.NumOfEntries > MAX_TRANSMIT_POWER_CONTROL_ENTRIES)
   {
      printf ("\nNumber of entries in TPC table exceeds the limit.\n");
      sleep(3);
      return TRUE;
   }

   if (TpcTable.NumOfEntries == 0)
   {
      printf ("\nThere is no entry in TPC table.\n");
      sleep(3);
      return TRUE;
   }

   int Value = (int)MasterBlaster->testCtrl.Power;

   if (ToggleMinMaxOption (&Value, Option, PO, 0, TpcTable.NumOfEntries-1,1))
   {
      MasterBlaster->testCtrl.Power = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterBdAddr (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   char Buffer[20];
   bdaddr_t bdaddr;

   printf ("\n Enter BdAddr: ");
//   gets(Buffer);
   scanf("%s",Buffer);
   str2ba(Buffer,&bdaddr);
   strlcpy(MasterBlaster->bdaddr,bdaddr.b,6);
   return TRUE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContTxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContTxMode;

   if (ToggleOption (&Value, Option, ContTxModeOption,
                        sizeof(ContTxModeOption)/sizeof(tMasterBlasterOption), CT,1))
   {
      MasterBlaster->ContTxMode = (UCHAR)Value;
      if (MasterBlaster->ContTxMode == ENABLE)
      {
         // Enable continous Tx should disable continous Rx
         MasterBlaster->ContRxMode = DISABLE;
	 MasterBlaster->LERxMode = DISABLE;
	 MasterBlaster->LETxMode = DISABLE;
	 LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContTxType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContTxType;

   if (ToggleOption (&Value, Option, ContTxTypeOption,
                        sizeof(ContTxTypeOption)/sizeof(tMasterBlasterOption), CX,1))
   {
      MasterBlaster->ContTxType = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterLERxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LERxMode;

   if (ToggleOption (&Value, Option, ContRxModeOption,
                       sizeof(ContRxModeOption)/sizeof(tMasterBlasterOption), LR, 1))
   {
      MasterBlaster->LERxMode = (UCHAR)Value;
      if (MasterBlaster->LERxMode == ENABLE)
      {
         /* Enable continous Tx should disable other modes */
         MasterBlaster->LETxMode = DISABLE;
         MasterBlaster->ContTxMode = DISABLE;
         MasterBlaster->ContRxMode = DISABLE;
         if (MasterBlaster->testCtrl.RxFreq > 39)
         {
            MasterBlaster->testCtrl.RxFreq = 39;
         }
         LEMode = TRUE;
      }
      else
      {
         LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterLETxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LETxMode;

   if (ToggleOption (&Value, Option, ContTxModeOption,
                sizeof(ContTxModeOption)/sizeof(tMasterBlasterOption), LT, 1))
   {
      MasterBlaster->LETxMode = (UCHAR)Value;
      if (MasterBlaster->LETxMode == ENABLE)
      {
         /* Enable continous Tx should disable other modes */
         MasterBlaster->LERxMode = DISABLE;
         MasterBlaster->ContTxMode = DISABLE;
         MasterBlaster->ContRxMode = DISABLE;
         if (MasterBlaster->testCtrl.TxFreq > MB_MAX_FREQUENCY_LE)
         {
            MasterBlaster->testCtrl.TxFreq = 39;
         }
         LEMode = TRUE;
      }
      else
      {
         LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}


//----------------------------------------------------------------------------

int SetMasterBlasterLETxPktPayload(tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = MasterBlaster->LETxParms.PktPayload;

   if (ToggleOption(&Value, Option, LETxPktPayloadOption,
                    sizeof(LETxPktPayloadOption)/sizeof(tMasterBlasterOption), LTM, 1))
   {
      MasterBlaster->LETxParms.PktPayload = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterContRxMode (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->ContRxMode;
   printf("\n N op\n");
   if (ToggleOption (&Value, Option, ContRxModeOption,
                        sizeof(ContRxModeOption)/sizeof(tMasterBlasterOption), CR,1))
   {
      MasterBlaster->ContRxMode = (UCHAR)Value;
      if (MasterBlaster->ContRxMode == ENABLE)
      {
         // Enable continous Tx should disable continous Rx
         MasterBlaster->ContTxMode = DISABLE;
	 MasterBlaster->LERxMode = DISABLE;
	 MasterBlaster->LETxMode = DISABLE;
	 LEMode = FALSE;
      }
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------
int SetMasterBlasterBERType (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   int Value = (int)MasterBlaster->BERType;
   if (ToggleOption (&Value, Option, BERPacketTypeOption,
			sizeof(BERPacketTypeOption)/sizeof(tMasterBlasterOption), SB, 1))
   {
      MasterBlaster->BERType = (UCHAR)Value;
      return TRUE;
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int SetMasterBlasterNothing (tBRM_Control_packet *MasterBlaster, tMasterBlasterOption *Option)
{
   UNUSED(MasterBlaster);
   UNUSED(Option);

   return TRUE;
}

//----------------------------------------------------------------------------

int ToggleOption (int *Value, tMasterBlasterOption *Option, tMasterBlasterOption *OptionArray,
                   int Size, int FieldID, int Step)
{
   char Opt = Option->Name[0];

   int Backward = ((Opt - 'A' + 'a') == MasterBlasterMenu[FieldID].Alias[0]);
   int i;
   for (i = 0; i < Size; ++i)
   {
      if (*Value == OptionArray[i].Value)
      {
         if (Backward)
         {
            i = ((i - Step) < 0) ? (Size - Step + i) : (i - Step);
         }
         else
         {
            i = (i + Step) % Size;
         }
         *Value = OptionArray[i].Value;
         return TRUE;
      }
   }
   return FALSE;
}

//----------------------------------------------------------------------------

int MinMaxOption (int *Value,  tMasterBlasterOption *Option, int Min, int Max)
{
   int NewValue = Option->Value;

   if (NewValue < Min || NewValue > Max)
   {
      return FALSE;
   }
   *Value = NewValue;
   return TRUE;
}

//----------------------------------------------------------------------------

int ToggleMinMaxOption (int *Value, char *Option, int FieldID, int Min, int Max, int Step)
{
   char Opt = *Option;
   int Backward = ((Opt - 'A' + 'a') == MasterBlasterMenu[FieldID].Alias[0]);

   if (Backward)
   {
      *Value = ((*Value - Step) < Min) ? (Max + 1 - (Step - (*Value - Min))) : (*Value - Step);
   }
   else
   {
      *Value = ((*Value + Step) > Max) ? (Min + (Step - (Max + 1 - *Value))) : (*Value + Step);
   }
   return TRUE;

}

//----------------------------------------------------------------------------
