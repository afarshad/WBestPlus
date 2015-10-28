// This file is modified by Arsham Farshad, Edinburgh University to accomodate Frame Aggregation in supporting IEEE 802.11n/ac standard and improve its performance.
// wbest_snd.c: the sender of WBest (V1.0) in Linux
//
// $Id: wbest_snd.c,v 1.8 2006/07/28 22:36:19 lmz Exp $
//
// Author:  Mingzhe Li (lmz@cs.wpi.edu)
//          Computer Science Dept.
//          Worcester Polytechnic Institute (WPI)
// WBEST+ Author: Arsham Farshad (arsham.farshad@gmail.com)
//        Informatics Dept., Edinburgh University, Scotland
//
// History
//   WBEST+ Version 1.2 - 2014
//
// Description:
//       wbest_snd.c
//              Sender of WBest, which is a bandwidth estimation tool
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

#include "wbest.h"
// global variable
char* lp_ServerName = "localhost";           // server name
int i_PortNumbUDP = 1234;                    // UDP port number
int i_PortNumbTCP = 9878;                    // TCP port number
int i_PktSize     = 1460;                    // packet size in bytes def:1460
int i_PktNumbPP   = 165;                      // number of packet pair 
int i_PktNumbBPP  = 33;                       // number of packet pairs that are sent together without pausing for pp_pausetime
int i_PktNumbPT   = 100;                      // length of packet train
int timer_resolution = 0;                    // local host select timer resolution
int gettimeofday_resolution = 0;             // local host gettimeofday resolution
int tcpSocket, udpSocket;                    // TCP/UDP socket
int i_rate = 0;   // For debug, if given in commandline, it overwrite the PP estimated capacity to send PT
int pp_pausetime=10000; // pause time between packet pairs transmission the default value is 10msec                                             

// Function prototype
void UDPClient(char *szServer, int i_PortNumb);              // Create UDP client
void TCPClient(char *szServer, int i_PortNumb);              // Create TCP client
double PerformEst(enum Options option, int num, double Rate);// Perform PP/PT test
void SendPP(int i_PktNumb, int burstsize);                   // Send PP over UDP
void SendPT(int i_PktNumb, double Rate);                     // Send PT over UDP
void ProbeTimer();                                             // Perform timer tests
void CleanUp(int);                                             // Clean up
void sort_int (int arr[], int num_elems);                      // bubble sorting for int



//////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  double ce=0.0, ab=0.0;
  struct timeval start, stop;
  int totalTime=0;
  
  //use getopt to parse the command line
  int c;

  while ((c = getopt(argc, argv, "p:h:s:n:m:r:f:x:")) != EOF)
    {
      switch (c)
        {
	case 'p':
	  printf("udp portnumber: %d\n", atoi(optarg));
	  i_PortNumbUDP = atoi(optarg);
	  break;
	  
	case 'h':
	  printf("hostname: %s\n", optarg);
	  lp_ServerName = optarg;
	  break;
	  
	case 's':
	  printf("packet size: %d\n", atoi(optarg));
	  i_PktSize = atoi(optarg);
	  if (i_PktSize > MAX_PKT_SIZE)
	    i_PktSize = MAX_PKT_SIZE;
	  break;
	    
	case 'n':
	  printf("number of packet pair: %d\n", atoi(optarg));
	  i_PktNumbPP = atoi(optarg);
	  break;
	
	case 'm':
	  printf("length of packet train: %d\n", atoi(optarg));
	  i_PktNumbPT = atoi(optarg);
	  if (i_PktNumbPT > MAX_PKT_NUM)
	    i_PktNumbPT = MAX_PKT_NUM;
	  break;

	case 'r':
	  printf("overwrite packet train rate: %d\n", atoi(optarg));
	  i_rate = atoi(optarg);
	  break;
	    //add by Arsham to get the pause time between packet pairs
	case 'f':
	  printf("overwrite packet pair pause time to:%d\n", atoi(optarg));
	  pp_pausetime = atoi(optarg);
	  break;

        case 'x':
	  printf("number of burst packet pairs: %d\n", atoi(optarg));
          if ( atoi(optarg) ){
              i_PktNumbBPP = atoi(optarg);
              if ( i_PktNumbBPP > i_PktNumbPP )
                  i_PktNumbBPP=i_PktNumbPP;
          }
	  break;


	case '?':
	  printf("ERROR: illegal option %s\n", argv[optind-1]);
	  printf("Usage:\n");
          printf("\t%s -p udp_portnumber -h hostname -s packet_size_bytes -n num_packet_pair -m train_length -r capacity -f pp pauseTime -x num_pp_in_burst \n", argv[0]);
	  exit(1); 
	  break;
	    
	default:
	  printf("WARNING: no handler for option %c\n", c);
	  printf("Usage:\n");
          printf("\t%s -p udp_portnumber -h hostname -s packet_size_bytes -n num_packet_pair -m train_length\n", argv[0]);
	  exit(1);
	  break;
        }
    }

  // Handle ctrl-C is not quit normally
  signal(SIGINT, CleanUp); 

  // Start the real business here...
  
  // setup TCP and UDP client
  TCPClient(lp_ServerName, i_PortNumbTCP); // Fixme to given port --lmz
  UDPClient(lp_ServerName, i_PortNumbUDP);

  // Probe the timer of this host
  ProbeTimer();
  
  // Measure proformance.
  gettimeofday(&start, NULL);

  ce = PerformEst(PacketPair, i_PktNumbPP, 0); 
  printf("Ce = %f\n", ce );

  ab = PerformEst (PacketTrain, i_PktNumbPT, ce);
  printf("Ab = %f\n", ab);

  // Measure performance
  gettimeofday(&stop, NULL);
  totalTime = (stop.tv_sec - start.tv_sec) * 1000000 +
    (stop.tv_usec - start.tv_usec);

  printf("Total estimation time: %d usec.\n", totalTime);

  CleanUp(0);
  return 0;
} // end main()

//////////////////////////////////////////////////////////////
void TCPClient(char *szServer, int nPort)
{
  struct sockaddr_in saTCPServer;              // TCP address
  struct hostent *hp;                          // Host entry

  bzero((void *) &saTCPServer, sizeof(saTCPServer));

  printf("Looking up TCP server %s...\n", szServer);
  if ((hp = gethostbyname(szServer)) == NULL) {
    perror("host name error");
    exit(1);
  }

  bcopy(hp->h_addr, (char *) &saTCPServer.sin_addr, hp->h_length);

  // Create a UDP/IP datagram socket
  tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
  if (tcpSocket < 0){
    perror("Failed in creating TCP socket");
    exit(1);
  }
   
  // Fill in the address structure for the server
  saTCPServer.sin_family = AF_INET;
  saTCPServer.sin_port = htons(nPort);// Port number from command line

  // Connect to TCP server
  if (connect(tcpSocket, (struct sockaddr *) &saTCPServer, sizeof(saTCPServer)) < 0 ) {
    perror("Failed in connect to TCP server");
    exit(1);
  }
  printf ("TCP connected to %s:%d\n", szServer, nPort);
  return;
} //end TCPClient()

////////////////////////////////////////////////////////////////////////
void UDPClient(char *szServer, int nPort)
{
  struct sockaddr_in saUDPServer;              // UDP address
  struct hostent *hp;                          // Host entry

  bzero((void *) &saUDPServer, sizeof(saUDPServer));
  printf("Looking up UDP %s...\n", szServer);
  if ((hp = gethostbyname(szServer)) == NULL) {
    perror("host name error");
    exit(1);
  }
  
  bcopy(hp->h_addr, (char *) &saUDPServer.sin_addr, hp->h_length);

  // Create a UDP/IP datagram socket
  udpSocket = socket(AF_INET,      // Address family
		     SOCK_DGRAM,   // Socket type
		     IPPROTO_UDP); // Protocol

  if (udpSocket < 0){
    perror("Failed in creating UDP socket");
    exit(1);
  }

  // Fill in the address structure for the server
  saUDPServer.sin_family = AF_INET;
  saUDPServer.sin_port = htons(nPort); // Port number from command line

  // UDP connection... not really needed. 
  if (connect(udpSocket, 
	      (struct sockaddr *) &saUDPServer, 
	      sizeof(saUDPServer)) < 0 ) {
    perror("Failed in connect to UDP server");
    exit(1);
    }
  printf ("UDP connected to %s:%d\n", szServer, nPort);
  return;
} // end UDPClient

/////////////////////////////////////////////////////////////////
double PerformEst(enum Options option, int i_num, double f_Rate) 
{
  int nRet;
  struct Ctl_Pkt control_pkt;
  control_pkt.value = i_num;
  control_pkt.option = option;

  if (send(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0) != sizeof(control_pkt))
    {
      perror("Send TCP control packet error");
      exit(1);
    }

  nRet = recv(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0);

  if (nRet!=sizeof(control_pkt) || control_pkt.option != Ready) 
    {
      printf("Receive unknow message %d, with size %d\n", control_pkt.option, nRet);
      exit(1);
    }

  if (option == PacketPair) 
    {
      SendPP(i_num,i_PktNumbBPP); // perform PP estimation
    }
  else if (option == PacketTrain) 
    {
      SendPT(i_num, f_Rate); // perform PT estimation
    }

  
  nRet = recv(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0);

  if (nRet!=sizeof(control_pkt) || control_pkt.option != option) 
    {
      printf("Receive unknow message %d, with size %d\n", control_pkt.option, nRet);
      exit(1);
    }

  return (double)control_pkt.value/1000000.;
  
}
/////////////////////////////////////////////////////////////////
// Sending Packet Pairs
/////////////////////////////////////////////////////////////////

void SendPP(int i_PktNumb,int burstsize) 
{
  struct timeval init_stamp, time_stamp;
  struct PP_Pkt pp_pkt; 
  int nRet=0;
  int i;
  pp_pkt.seq = 0;
  pp_pkt.bseq= 0;
  gettimeofday(&init_stamp, NULL);

  // 
  // start here: packetpair in a burst have the same bseq bumber and different seq number.
  //


  for (i=0; i < i_PktNumb; i++) {

	  if ( !(i % burstsize) ){
		  usleep (pp_pausetime);
		  pp_pkt.bseq++;
	  }

    gettimeofday(&time_stamp, NULL);
    pp_pkt.tstamp = (time_stamp.tv_sec - init_stamp.tv_sec) * 1000000 +
                    (time_stamp.tv_usec - init_stamp.tv_usec);
    nRet = send(udpSocket, (char *) &pp_pkt, i_PktSize, 0);
    //printf("(%d,%d)\n",pp_pkt.bseq, pp_pkt.seq); 

    pp_pkt.seq++;

  if (nRet < 0)
    {
      perror("Packet send error");
      close(udpSocket);
      exit(1);
    }//
  }
} // end SendPP()
/////////////////////////////////////////////////////////////////
void SendPT(int i_PktNumb, double f_Rate)
{
  // All time values are in usec
  double totalTime = 0.;      // time to send i_PktNumb packets in total
  double avgPacketTime = 0.;  // computed average time for sending one packet 
  double sleepTime = 0.;         // Select timeout value
  double initTime = 0., doneTime = 0.;   // Start and end time
  struct timeval initTV, doneTV;    // Start and end time
  double tempTime1 = 0., tempTime2 = 0.; // Used to control sending rate
  struct PP_Pkt pt_pkt;
  double avgSendRate = 0.;
  int nRet = 0, i=0;

  if (i_rate != 0) f_Rate = (double) i_rate; // for debug, overwrite the estimated effective capacity

  // time from the start of first packet to the start of the last packet
  totalTime = (double) ((i_PktNumb - 1)  * i_PktSize *8)/ (f_Rate); //usec
  avgPacketTime = totalTime / (double)(i_PktNumb - 1); // usec


  printf("PacketTrain: sending %d PT with %f us per packet, at %f Mbps\n", i_PktNumb, avgPacketTime, f_Rate);

  if (avgPacketTime < gettimeofday_resolution || f_Rate < VERY_SMALL || i_PktNumb <= 0) // have problems
    {
      printf("Can not send %d PT with %f us per packet at %f Mbps. \n", i_PktNumb, avgPacketTime, f_Rate);
      return;
    }

  pt_pkt.seq = 0;
  pt_pkt.bseq=0;
  pt_pkt.tstamp = 0;
  gettimeofday(&initTV, NULL);
  initTime = tempTime1 = (double) initTV.tv_sec * 1000000. + (double)initTV.tv_usec;

  for (i=0; i < i_PktNumb; i++) 
    {
      nRet = send(udpSocket, (char *) &pt_pkt, i_PktSize, 0);
      gettimeofday(&doneTV, NULL);
      tempTime2 = (double) (doneTV.tv_sec * 1000000.) +  (double) doneTV.tv_usec;
      sleepTime = avgPacketTime - (tempTime2 - tempTime1)-timer_resolution;
      //sleepTime = avgPacketTime;
      // Use the select timer first
      if (sleepTime > 1.5 * timer_resolution ) 
	{
	  doneTV.tv_sec = (int)sleepTime / 1000000;
	  //doneTV.tv_usec = ((int)sleepTime % 1000000)/timer_resolution*timer_resolution; // explain it 
	  doneTV.tv_usec = (int)sleepTime % 1000000;  
          select(0,NULL,NULL,NULL,&doneTV);   // Sleep 
	  gettimeofday(&doneTV, NULL);          // get the return time
	  tempTime2 = (double)doneTV.tv_sec * 1000000. +  (double)doneTV.tv_usec;
	  sleepTime = avgPacketTime - (tempTime2 - tempTime1);
	}
      // Let the busy waiting time handle the small ammount of sleep time
      while (sleepTime > gettimeofday_resolution)
	{
	  gettimeofday(&doneTV, NULL);          
          tempTime2 = (double)doneTV.tv_sec * 1000000. +  (double)doneTV.tv_usec;
          sleepTime = avgPacketTime - (tempTime2 - tempTime1);
	}

      pt_pkt.seq++;
      pt_pkt.tstamp = (doneTV.tv_sec - initTV.tv_sec) * 1000000 +  doneTV.tv_usec - initTV.tv_usec;
      tempTime1 = tempTime2;
    }
  doneTime = tempTime2;
  avgSendRate = ((i_PktNumb) * i_PktSize * 8)/(doneTime - initTime);
  printf("Real sending rate: %f Mbps, time spend: %f us, average packet time %f us\n", 
	 avgSendRate, doneTime - initTime, (doneTime - initTime)/i_PktNumb);

} // end SendPT

/////////////////////////////////////////////////////////////////
void sort_int (int arr[], int num_elems)
{
  int i,j;
  int temp;

  for (i=1; i<num_elems; i++) {
    for (j=i-1; j>=0; j--)
      if (arr[j+1] < arr[j])
	{
	  temp = arr[j];
	  arr[j] = arr[j+1];
	  arr[j+1] = temp;
	}
      else break;
  }
} // end sort_int()

/////////////////////////////////////////////////////////////////
void ProbeTimer() // Some ideas from pathload take the median as the timer resolution.
{
  int probe[NUM_TIMER_PROBING];
  int i;
  struct timeval sleep_time, time[NUM_TIMER_PROBING] ;

  // Probe the Select timer resolution
  gettimeofday(&time[0], NULL);
  for(i=1; i<NUM_TIMER_PROBING; i++) // Give a sleep time to see how fast we can get back
    {
      sleep_time.tv_sec = 0;
      sleep_time.tv_usec = 1;
      select(0,NULL,NULL,NULL,&sleep_time);  // Sleep for 1 usec
      gettimeofday(&time[i], NULL);          // get the return time
    }

  for(i=1; i<NUM_TIMER_PROBING; i++)
    {
      probe[i-1] = (time[i].tv_sec - time[i-1].tv_sec) * 1000000 +
	           (time[i].tv_usec - time[i-1].tv_usec);
    }

  sort_int(probe, NUM_TIMER_PROBING - 1);  // sort the timer probing

  timer_resolution = (probe[NUM_TIMER_PROBING/2]+probe[NUM_TIMER_PROBING/2+1])/2;

  printf("The timer resolution is %d usecs.\n", timer_resolution);

  // Probe the gettimeofday_resolution.
  gettimeofday(&time[0], NULL);         
  for(i=1; i<NUM_TIMER_PROBING; i++) // Give a sleep time to see how fast we can get back
    {
      gettimeofday(&time[i], NULL);          // get the return time
      probe[i-1] = (time[i].tv_sec - time[i-1].tv_sec) * 1000000 +
	(time[i].tv_usec - time[i-1].tv_usec);
    }

  sort_int(probe, NUM_TIMER_PROBING - 1);  // sort the timer probing
  gettimeofday_resolution = (probe[NUM_TIMER_PROBING/2]+probe[NUM_TIMER_PROBING/2+1])/2;
  //  gettimeofday_resolution = gettimeofday_resolution < 2 ? 2 : gettimeofday_resolution; // 1 make no sense

  printf("The gettimeofday resolution is %d usecs.\n", gettimeofday_resolution);

} // end ProbeTimer()

//////////////////////////////////////////////////////////////////////
void CleanUp(int arg1){
  printf("WBest sender is now off\n");
  close(tcpSocket);
  close(udpSocket);
  exit(0);
} // end CleanUp()
