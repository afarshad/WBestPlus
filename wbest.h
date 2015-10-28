// This file is modified by Arsham Farshad, Edinburgh University to accomodate Frame Aggregation in supporting IEEE 802.11n/ac standard and improve its performance.

// wbest.h: the header file of WBest (V1.0) in Linux
//
// $Id: wbest.h,v 1.7 2006/07/28 20:14:28 lmz Exp $
//
// Author:  Mingzhe Li (lmz@cs.wpi.edu)
//          Computer Science Dept.
//          Worcester Polytechnic Institute (WPI)
// WBEST+ Author: Arsham Farshad (arsham.farshad@gmail.com)
//        Informatics Dept., Edinburgh University, Scotland
//
//
// History
//   WBEST+ Version 1.2 - 2014
//

//
// Description:
//       wbest.h
//              Header file of WBest, which is a bandwidth estimation tool
//              designed for wireless streaming application.
//              Wireless Bandwidth ESTimation (WBest)
//
// Compile and Link:
//       make
//
// License:
//   This software is released into the public domain.
//   You are free to use it in any way you like.
//   This software is provided "as is" with no expressed
//   or implied warranty.  I accept no liability for any
//   damage or loss of business that this software may cause.
////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#define MAX_PKT_SIZE 4096      // Max pp packet size
#define MAX_PKT_SIZE 1470      // Max pp packet size
#define MAX_PKT_NUM 200       // Max number PP in a bursty
#define VERY_SMALL 1e-20       // Very small
#define NUM_TIMER_PROBING  31  // Number of timer probing, used in ProbeTimer()
#define MIN_DISPERSION_TIME_PP  300 //Minimum dispersion time is 8*1470/200Mbps for the smaller packet size it is going be smaller which is not considered for now. 
#define MIN_DISPERSION_TIME 300  //Minimum dispersion time is 8*1470/200Mbps for the smaller packet size it is going be smaller which is not considered for now. 
#define THRESHOLD_PERCENT	90 // 90% of sorted disperse array
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff /* should be in <netinet/in.h> */
#endif

enum Options {                  // Options used in control packet
    PacketPair = 0x0001
  , PacketTrain = 0x0002
  , Ready = 0x0004
  , Failed = 0x0008
};

struct Ctl_Pkt{                // Control packet (over TCP)
  enum Options option;
  unsigned int value;
};

struct PP_Pkt{                 // PP/PT packet (over UDP)
  int seq;
  int tstamp;
  int bseq;                    // contains burst seq number which is for burst transmission.
  unsigned char padding[MAX_PKT_SIZE];
};


