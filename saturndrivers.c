/* Copyright (C)
* 2021 - Laurence Barker G8NJJ
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

/////////////////////////////////////////////////////////////
//
// Saturn project: Artix7 FPGA + Raspberry Pi4 Compute Module
// PCI Express interface from linux on Raspberry pi
// this application uses C code to emulate HPSDR protocol 1
//
// Contribution of interfacing to PiHPSDR from N1GP (Rick Koch)
//
// saturndrivers.c:
// Drivers for minor IP cores
//
//////////////////////////////////////////////////////////////


#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <stdlib.h>                     // for function min()
#include <math.h>
#include "saturndrivers.h"
//#include "saturnregisters.h"
#include <semaphore.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "message.h"

#define VMEMBUFFERSIZE 32768                    // memory buffer to reserve
#define AXIBaseAddress 0x10000                  // address of StreamRead/Writer IP

// START hwaccess.c
//
// mem read/write variables:
//
int register_fd;                             // device identifier




//
// open connection to the XDMA device driver for register and DMA access
//
int OpenXDMADriver(void) {
  int Result = 0;

  if ((register_fd = open("/dev/xdma0_user", O_RDWR)) == -1) {
    t_print("register R/W address space not available\n");
  } else {
    t_print("register access connected to /dev/xdma0_user\n");
    Result = 1;
  }

  return Result;
}




//
// initiate a DMA to the FPGA with specified parameters
// returns 1 if success, else 0
// fd: file device (an open file)
// SrcData: pointer to memory block to transfer
// Length: number of bytes to copy
// AXIAddr: offset address in the FPGA window
//
int DMAWriteToFPGA(int fd, unsigned char*SrcData, uint32_t Length, uint32_t AXIAddr) {
  ssize_t rc;                 // response code
  off_t OffsetAddr;
  OffsetAddr = AXIAddr;
  rc = lseek(fd, OffsetAddr, SEEK_SET);

  if (rc != OffsetAddr) {
    t_print("seek off 0x%lx != 0x%lx.\n", rc, OffsetAddr);
    t_perror("seek file");
    return -EIO;
  }

  // write data to FPGA from memory buffer
  rc = write(fd, SrcData, Length);

  if (rc < 0) {
    t_print("write 0x%x @ 0x%lx failed %ld.\n", Length, OffsetAddr, rc);
    t_perror("DMA write");
    return -EIO;
  }

  return 0;
}

//
// initiate a DMA from the FPGA with specified parameters
// returns 1 if success, else 0
// fd: file device (an open file)
// DestData: pointer to memory block to transfer
// Length: number of bytes to copy
// AXIAddr: offset address in the FPGA window
//
int DMAReadFromFPGA(int fd, unsigned char*DestData, uint32_t Length, uint32_t AXIAddr) {
  ssize_t rc;                 // response code
  off_t OffsetAddr;
  OffsetAddr = AXIAddr;
  rc = lseek(fd, OffsetAddr, SEEK_SET);

  if (rc != OffsetAddr) {
    t_print("seek off 0x%lx != 0x%lx.\n", rc, OffsetAddr);
    t_perror("seek file");
    return -EIO;
  }

  // write data to FPGA from memory buffer
  rc = read(fd, DestData, Length);

  if (rc < 0) {
    t_print("read 0x%x @ 0x%lx failed %ld.\n", Length, OffsetAddr, rc);
    t_perror("DMA read");
    return -EIO;
  }

  return 0;
}

//
// 32 bit register read over the AXILite bus
//
uint32_t RegisterRead(uint32_t Address) {
  uint32_t result = 0;
  ssize_t nread = pread(register_fd, &result, sizeof(result), (off_t) Address);

  if (nread != sizeof(result)) {
    t_print("ERROR: register read: addr=0x%08X   error=%s\n", Address, strerror(errno));
  }

  return result;
}

//
// 32 bit register write over the AXILite bus
//
void RegisterWrite(uint32_t Address, uint32_t Data) {
  ssize_t nsent = pwrite(register_fd, &Data, sizeof(Data), (off_t) Address);

  if (nsent != sizeof(Data)) {
    t_print("ERROR: Write: addr=0x%08X   error=%s\n", Address, strerror(errno));
  }
}
// END hwaccess.c


sem_t DDCResetFIFOMutex;

//
// DMA FIFO depths
// this is the number of 64 bit FIFO locations
//
uint32_t DMAFIFODepths[VNUMDMAFIFO] = {
  8192,             //  eRXDDCDMA,    selects RX
  1024,             //  eTXDUCDMA,    selects TX
  256,              //  eMicCodecDMA, selects mic samples
  256               //  eSpkCodecDMA  selects speaker samples
};


//
// void SetupFIFOMonitorChannel(EDMAStreamSelect Channel, bool EnableInterrupt);
//
// Setup a single FIFO monitor channel.
//   Channel:     IP channel number (enum)
//   EnableInterrupt: true if interrupt generation enabled for overflows
//
void SetupFIFOMonitorChannel(EDMAStreamSelect Channel, bool EnableInterrupt) {
  uint32_t Address;             // register address
  uint32_t Data;                // register content
  Address = VADDRFIFOMONBASE + 4 * Channel + 0x10;      // config register address
  Data = DMAFIFODepths[(int)Channel];             // memory depth

  if ((Channel == eTXDUCDMA) || (Channel == eSpkCodecDMA)) { // if a "write" FIFO
    Data += 0x40000000;  // bit 30
  }

  if (EnableInterrupt) {
    Data += 0x80000000;  // bit 31
  }

  RegisterWrite(Address, Data);
}



//
// uint32_t ReadFIFOMonitorChannel(EDMAStreamSelect Channel, bool* Overflowed);
//
// Read number of locations in a FIFO
// for a read FIFO: returns the number of occupied locations available to read
// for a write FIFO: returns the number of free locations available to write
//   Channel:     IP core channel number (enum)
//   Overflowed:    true if an overflow has occurred. Reading clears the overflow bit.
//
uint32_t ReadFIFOMonitorChannel(EDMAStreamSelect Channel, bool* Overflowed) {
  uint32_t Address;             // register address
  uint32_t Data = 0;              // register content
  bool Overflow = false;
  Address = VADDRFIFOMONBASE + 4 * (uint32_t)Channel;     // status register address
  Data = RegisterRead(Address);

  if (Data & 0x80000000) {                  // if top bit set, declare overflow
    Overflow = true;
  }

  Data = Data & 0xFFFF;                   // strip to 16 bits
  *Overflowed = Overflow;                   // send out overflow result

  if ((Channel == eTXDUCDMA) || (Channel == eSpkCodecDMA)) { // if a write channel
    Data = DMAFIFODepths[Channel] - Data;  // calculate free locations
  }

  return Data;                        // return 16 bit FIFO count
}





//
// reset a stream FIFO
//
void ResetDMAStreamFIFO(EDMAStreamSelect DDCNum) {
  uint32_t Data;                    // DDC register content
  uint32_t DataBit = 0;

  switch (DDCNum) {
  case eRXDDCDMA:             // selects RX
    DataBit = (1 << VBITDDCFIFORESET);
    break;

  case eTXDUCDMA:             // selects TX
    DataBit = (1 << VBITDUCFIFORESET);
    break;

  case eMicCodecDMA:            // selects mic samples
    DataBit = (1 << VBITCODECMICFIFORESET);
    break;

  case eSpkCodecDMA:            // selects speaker samples
    DataBit = (1 << VBITCODECSPKFIFORESET);
    break;
  }

  sem_wait(&DDCResetFIFOMutex);                       // get protected access
  Data = RegisterRead(VADDRFIFORESET);        // read current content
  Data = Data & ~DataBit;
  RegisterWrite(VADDRFIFORESET, Data);        // set reset bit to zero
  Data = Data | DataBit;
  RegisterWrite(VADDRFIFORESET, Data);        // set reset bit to 1
  sem_post(&DDCResetFIFOMutex);                       // release protected access
}



//
// SetTXAmplitudeEER (bool EEREnabled)
// enables amplitude restoratino mode. Generates envelope output alongside I/Q samples.
// NOTE hardware does not properly support this yet!
//
void SetTXAmplitudeEER(bool EEREnabled) {
  //GEEREnabled = EEREnabled;               // save value
  //HandlerSetEERMode(EEREnabled);              // I/Q send handler
}


//
// number of samples to read for each DDC setting
// these settings must match behaviour of the FPGA IP!
// a value of "7" indicates an interleaved DDC
// and the rate value is stored for *next* DDC
//
const uint32_t DDCSampleCounts[] = {
  0,            // set to zero so no samples transferred
  1,
  2,
  4,
  8,
  16,
  32,
  0           // when set to 7, use next value & double it
};

//
// uint32_t AnalyseDDCHeader(unit32_t Header, unit32_t** DDCCounts)
// parameters are the header read from the DDC stream, and
// a pointer to an array [DDC count] of ints
// the array of ints is populated with the number of samples to read for each DDC
// returns the number of words per frame, which helps set the DMA transfer size
//
uint32_t AnalyseDDCHeader(uint32_t Header, uint32_t* DDCCounts) {
  uint32_t DDC;               // DDC counter
  uint32_t Count;
  uint32_t Total = 0;

  for (DDC = 0; DDC < VNUMDDC; DDC++) {
    // 3 bit value for this DDC
    uint32_t Rate = Header & 7;            // get settings for this DDC

    if (Rate != 7) {
      Count = DDCSampleCounts[Rate];
      DDCCounts[DDC] = Count;
      Total += Count;           // add up samples
    } else {              // interleaved
      Header = Header >> 3;
      Rate = Header & 7;          // next 3 bits
      Count = 2 * DDCSampleCounts[Rate];
      DDCCounts[DDC] = Count;
      Total += Count;
      DDCCounts[DDC + 1] = 0;
      DDC += 1;
    }

    Header = Header >> 3;         // ready for next DDC rate
  }

  return Total;
}
