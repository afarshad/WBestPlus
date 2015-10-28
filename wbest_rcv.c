// This file is modified by Arsham Farshad, Edinburgh University to accomodate Frame Aggregation in supporting IEEE 802.11n/ac standard and improve its performance.

// wbest_rcv.c: the receiver of WBest (V1.0) in Linux
//
// $Id: wbest_rcv.c,v 1.7 2006/07/28 19:49:59 lmz Exp $
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
// Description:
//       wbest_rcv.c
//              Receiver of WBest, which is a bandwidth estimation tool
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
int tcpSocket, udpSocket;                  // TCP/UDP socket  
struct timeval arrival[MAX_PKT_NUM];       // arrrival time
struct timeval karrival[MAX_PKT_NUM];      // kernel arrival timestamp 
int sendtime[MAX_PKT_NUM];                 // sending time
int seq[MAX_PKT_NUM];                      // sequence number
int bseq[MAX_PKT_NUM];                     // burst sequence number
int fa[MAX_PKT_NUM];                       //  fa[i]=1 if packet is aggregated with other packet. default value is 0. 
int psize[MAX_PKT_NUM];                    // packet size (bytes)
int disperse[MAX_PKT_NUM];                 // dispersion(usec)
int kdisperse[MAX_PKT_NUM];                 // dispersion(usec) calculate based on the kernel time stamp.
double ce[MAX_PKT_NUM];                    // Effective capacity (Mbps)
double kce[MAX_PKT_NUM];                    // Effective capacity (Mbps) calculated based onthe kernel timestamp
double sr[MAX_PKT_NUM];                    // Sending rate (Mbps)
int ceflag[MAX_PKT_NUM];                   // valid packet pair
double allCE = 0., allAT = 0., allAB =0.;
char  *file_name="rcvlog.csv";             // the file keeps ce and disperse array
int  txrateRplc=1;			   // If At > Sr then it replaces Tx dispersion time with dispersion time calculated from receiver, default value is 1. 
char  pt_file_name[100]="ptlog.csv";           // the file keeps packet train log 
char  dthist_file_name[100]="dthist.csv";           // the file keeps th histogram data for dispersion time for  packet train
char  pp_file_name[100]="pplog.csv";           // the file keeps packet pair dispersion time log 
int     burstSize=1;                       // bursatSize is specified in the sender but only accept here to store in log file
double effective_capacity=0.;              // instead of comparing the At to Ce from PPprocess take this number to comapre 
int  aggrThr=MIN_DISPERSION_TIME;
int  aggrThrPP=MIN_DISPERSION_TIME_PP;  //aggregate threshold for the packet pair
// Function prototype
void TCPServer(int nPort);                             // setup the TCP server 
void UDPServer(int nPort);                             // the UDP server 
void UDPReceive (enum Options option, int i_PktNumb);  // Receive UDP packet (PP/PT) 
double ProcessPP (int i_PktNumb);                      // Process PP data
double ProcessPPwFa (int i_PktNumb);                      // Process PP data
double ProcessPT (int i_PktNumb);                      // Process PT data
double ProcessPTdr (int i_PktNumb);                      // Process PT data
void InitStorage() ;                                   // initial array
int FrameAggThr(int dt[],int num_elems,int perc);      // Calculate the threshold for filtering out aggregated frames.
void CleanUp(int arg);                                     // handle for ctrl_c, clean up
void sort_int (int arr[], int num_elems);              // Sort int array
void sort_double (double arr[], int num_elems);        // Sort double array

//////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  int nRet = 0;
  int nPortUDP = 1234;
  int nPortTCP = 9878;
  int i_PktNumb; 
  struct Ctl_Pkt control_pkt;
  int c;

  //use getopt to parse the command line
  while ((c = getopt(argc, argv, "p:f:x:r:d:t:b:")) != EOF)
    {
      switch (c)
        {
        case 'p':
          printf("udp portnumber: %d\n", atoi(optarg));
          nPortUDP = atoi(optarg);
          break;

        case 'x':
          if (atoi(optarg) > 0 ){
          printf("Overwrite Burst Size: %d\n", atoi(optarg));
          burstSize = atoi(optarg);
          }
          break;

        case 'r':
          if ( atof(optarg) > 0.0 ){
          printf("Overwrite Effective Capacity to: %f\n", atof(optarg));
          effective_capacity=atof(optarg);
          }
          break;

        case 'f':
          file_name = optarg;
          strcpy(pt_file_name,"");
          strcat(pt_file_name,file_name);
          strcat(pt_file_name,"_pt.csv");
          strcpy(pp_file_name,"");
          strcat(pp_file_name,file_name);
          strcat(pp_file_name,"_pp.csv");
          strcpy(dthist_file_name,"");
          strcat(dthist_file_name,file_name);
          strcat(dthist_file_name,"_dthist.csv");
          break;
        case 'd':
	  if (atoi(optarg) == 0){
          printf("disable Tx dispersion replacement: %d\n", atoi(optarg));
          txrateRplc = atoi(optarg);
	  }
          break;

        case 't':
	  if (atoi(optarg) > 0){
          printf("Overwrite the aggregation threshold to:%d\n", atoi(optarg));
          aggrThr= atoi(optarg);
	  }
          break;
        case 'b':
	  if (atoi(optarg) > 0){
          printf("Overwrite the aggregation threshold for PP to:%dusec\n", atoi(optarg));
          aggrThrPP= atoi(optarg);
	  }
          break;
        case '?':
          printf("ERROR: illegal option %s\n", argv[optind-1]);
          printf("Usage:\n");
          printf("\t%s -p udp_portnumber -f log_file -x [burst_size|max_agg_fr_pt] -r effective_capacity -d [disable Snd dispersion replacement:0] -t aggregation_Threshold_usec \n", argv[0]);
          exit(1);
          break;

        default:
          printf("WARNING: no handler for option %c\n", c);
          printf("Usage:\n");
          printf("\t%s -p udp_portnumber -f log_file -x burst_size -r effective_capacity\n", argv[0]);
          exit(1);
          break;
        }
    }
  

  signal(SIGINT, CleanUp);

  UDPServer(nPortUDP);
  TCPServer(nPortTCP);

  while(1) 
    {
      nRet = recv(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0);
      if (nRet <= 0 ) // peer closed
	{
	  break;
	}
      
      // what does the client want to do? PP/PT?
      if (nRet!=sizeof(control_pkt) || (control_pkt.option != PacketPair && control_pkt.option!=PacketTrain)) 
	{
	  printf("Receive unknow message %d, with size %d\n", control_pkt.option, nRet);
	  exit(1);
	}

      i_PktNumb = control_pkt.value;
  
      switch (control_pkt.option) 
	{
	case PacketPair:
	  control_pkt.option = Ready;
	  if (send(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0) != sizeof(control_pkt))
	    {
	      perror("Send TCP control packet error");
	      exit(1);
	    }
	  // receive PP
	  UDPReceive(PacketPair, i_PktNumb); 

	  control_pkt.option = PacketPair;
	  //allCE = ProcessPP(i_PktNumb); 
	  allCE = ProcessPPwFa(i_PktNumb); 
	  control_pkt.value = (unsigned int) (allCE * 1000000);

	  if (send(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0) != sizeof(control_pkt))
	    {
	      perror("Send TCP control packet error");
	      exit(1);
	    }
	  break;

	case PacketTrain:
	  control_pkt.option = Ready;
	  if (send(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0) != sizeof(control_pkt))
	    {
	      perror("Send TCP control packet error");
	      exit(1);
	    }

	  // receive PT
	  UDPReceive(PacketTrain, i_PktNumb);  

	  control_pkt.option = PacketTrain;
	  allAB = ProcessPT(i_PktNumb);
	  //allAB = ProcessPTdr(i_PktNumb);
      
	  control_pkt.value = (unsigned int)(allAB * 1000000);

	  if (send(tcpSocket, (char *) &control_pkt, sizeof(control_pkt), 0) != sizeof(control_pkt))
	    {
	      perror("Send TCP control packet error");
	      exit(1);
	    }
	  break;

	default:
	  break;
	}
    } //end while(1)

  CleanUp (0);
  return 0;
} // end main()

////////////////////////////////////////////////////////////////////////////////////
void TCPServer (int nPort)
{
  int listenSocket;
  struct sockaddr_in saTCPServer, saTCPClient;              // TCP address
  int nRet;                                                 // result
  int nLen;                                                 // length
  char szBuf[4096];                                         // client name

  listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket < 0)
    {
      perror("Failed to create listensocket create");
      exit(1);
    }

  saTCPServer.sin_family = AF_INET;
  saTCPServer.sin_addr.s_addr = INADDR_ANY; // Let WinSock assign address
  saTCPServer.sin_port = htons(nPort);   // Use port passed from user


  // bind the name to the socket
  nRet = bind(listenSocket, (struct sockaddr *) &saTCPServer, sizeof(struct sockaddr));
  if (nRet < 0)
    {
      perror ("Bind TCP server socket failed");
      close(listenSocket);
      exit(1);
    }
  
  if (listen(listenSocket, 5) < 0) {
    perror ("Listen Failed");
    exit(1);
  }

  // printing out where the server is waiting
  nRet = gethostname(szBuf, sizeof(szBuf));
  if (nRet < 0)
    {
      perror("Failed to get the server name");
      close(listenSocket);
      exit(1);
    }

  // Show the server name and port number
  printf("\nTCP Server named %s listening on port %d\n", szBuf, nPort);
  
  /* Set the size of the in-out parameter */
  nLen = sizeof(saTCPClient);

  /* Wait for a client to connect */
  tcpSocket = accept(listenSocket, (struct sockaddr *) &saTCPClient,  &nLen);
  if (tcpSocket < 0)
    {
      perror("Failed to sccept client connection");
      close(listenSocket);
      exit(1);
    }
  
  close (listenSocket);
  
} // end TCPServer()

/////////////////////////////////////////////////////////////////////////////
void UDPServer(int nPort)
{

  int nRet;
  struct sockaddr_in saUDPServer;
  //char szBuf[4096];
  char szBuf[2000];

  udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if (udpSocket < 0)
    {
      perror("Failed to create UDP socket");
      exit(1);
    }

  // Fill in the address structure
  saUDPServer.sin_family = AF_INET;
  saUDPServer.sin_addr.s_addr = INADDR_ANY; // Let Socket assign address
  saUDPServer.sin_port = htons(nPort);      // Use port passed from user
 // Add the timestamp in the kernell level
int timestampOn = 1;
int rc=0;
    rc = setsockopt(udpSocket, SOL_SOCKET, SO_TIMESTAMP, (int *) &timestampOn, sizeof(timestampOn));
    if (0 > rc)
    {
        perror("setsockopt(SO_TIMESTAMP) failed.\n");
        close (udpSocket);
	exit(1);
    }


  // bind the name to the socket
  nRet = bind(udpSocket, (struct sockaddr *) &saUDPServer, sizeof(struct sockaddr));
  if (nRet < 0)
    {
      perror ("Failed to bind UDP socket");
      close (udpSocket);
      exit(1);
    }


  // Show where the server is waiting
  nRet = gethostname(szBuf, sizeof(szBuf));
  if (nRet < 0)
    {
      perror ("Failed to get local host name");
      close (udpSocket);
      exit(1);
    }

  // Show the server name and port number
  printf("UDP Server named %s waiting on port %d\n",
	 szBuf, nPort);
} // end UDPServer()

/////////////////////////////////////////////////////////////////////////////
void UDPReceive (enum Options option, int i_PktNumb)
{
  int nLen, inum = 0, nSelect = 0;
  //char szBuf[4096];
  char szBuf[MAX_PKT_SIZE];
  int nRet, errgen=0;
  struct sockaddr_in saUDPClient;
  struct timeval timeout,time_kernel;           // Timeout for select
  fd_set rfds;
  nLen = sizeof(saUDPClient);

  // Initial storage, wait for data from the client
  InitStorage(); 

  // if packetpair, double the packet number.
  if (option == PacketPair) i_PktNumb = 2 * i_PktNumb;

  // Use select to control timeout for 300 ms
  FD_ZERO(&rfds);
  FD_SET(udpSocket, &rfds);

  struct msghdr   msg;
  char   ctrl[CMSG_SPACE(sizeof(struct timeval))];
  struct cmsghdr *cmsg = (struct cmsghdr *) &ctrl;
  struct iovec    iov;
  msg.msg_control      = (char *) ctrl;
  msg.msg_controllen   = sizeof(ctrl);    
  msg.msg_name         = &saUDPClient;
  msg.msg_namelen      = nLen;
  msg.msg_iov          = &iov;
  msg.msg_iovlen       = 1;
  iov.iov_base         = szBuf;
  iov.iov_len          = sizeof(szBuf);
  
while(inum < i_PktNumb){
    timeout.tv_sec = 0;             // reset the timout value
    timeout.tv_usec = 300000;       // to 300 ms

    memset(szBuf, 0, sizeof(szBuf));
    nSelect = select(udpSocket+1, &rfds, NULL, NULL, &timeout);
    if ( nSelect < 0) {
      perror("Failed in select function");
      break;
    } 
    else if (nSelect == 0) {
      printf("Receiving UDP packets timeout (300 ms).\n");
      break;
    }
    else {

      nRet = recvmsg(udpSocket, 
		      &msg,
		      0);

      if (nRet > 0) {
	gettimeofday(&arrival[inum], NULL);
	struct timeval time_kernel;
	if (cmsg->cmsg_level == SOL_SOCKET &&
			cmsg->cmsg_type  == SCM_TIMESTAMP &&
			cmsg->cmsg_len   == CMSG_LEN(sizeof(time_kernel))) {
		memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
		karrival[inum]=time_kernel;
	}
        
	seq[inum] = ((struct PP_Pkt *) &szBuf)->seq;
	bseq[inum] = ((struct PP_Pkt *) &szBuf)->bseq;
	sendtime[inum] = ((struct PP_Pkt *) &szBuf)->tstamp;
	psize[inum] = nRet;
	if (seq[inum] == i_PktNumb - 1)
	 break;
	inum++;
      }
    }
  }
  return;

} // end UDPReceive()

/////////////////////////////////////////////////////////////////////
//////////////////////// WBEST+ Packet Pair /////////////////////////
//
//*******************************************************
// It accounts the FrameAggregation

double ProcessPPwFa (int i_PktNumb)
{
	// Show that we've received some data
	int processed = 0, count=0, i=0;
	double sum = 0., sum_disperse=0., mean = 0., median=0.,kmedian=0.,kmean=0.,sum_kdisperse=0.;
	FILE *fp;
	int disperse_temp=0;
	int fa_count=1,pp_count=0;
        int nfa_count=0;
        int overhead=0;

	for (i=0; i<i_PktNumb; i++) 
	{

			if ( processed > seq[i]) continue; // duplicated packet
			while (processed < seq[i])        // we skip any pairs?
			{
				ceflag[processed] = 0;                               // this pair is not valid
				printf("[%2d]: Packet pair lost\n", processed);
				processed ++;
			}

		while (bseq[i] == bseq[i+1] && bseq[i] >= 0 && i < i_PktNumb-1 ) //packet pairs are belongs to one burst

		{
			ceflag[processed] = 1;     // this pair is valid
			disperse[i] = (karrival[i+1].tv_sec-karrival[i].tv_sec)*1000000+(karrival[i+1].tv_usec - karrival[i].tv_usec);
			//disperse[i] = (arrival[i+1].tv_sec-arrival[i].tv_sec)*1000000+(arrival[i+1].tv_usec - arrival[i].tv_usec);
			//disperse_temp+=disperse[i];
                        printf("dipsperse[%d]=%d\n",i,disperse[i]);

			if ( disperse[i] < aggrThrPP ){
				fa_count++;
                                overhead+=disperse[i];
			}
                        else{
                        nfa_count++;    
			disperse_temp=disperse[i];
                        ce[count] = (double)(psize[i]*8*fa_count)/(double)(disperse_temp+overhead);              // compute effective capacity
                printf ("ce[%d]=%d*8*%d/(%d+%d): %.2f\n",count,psize[i],fa_count,disperse_temp,overhead,ce[count]);
		fa_count=1;
                overhead=0;
		count ++ ; // increase valid packet pair by 1
                        }
                //        printf("%d:disperse[%d]:%d,disperse_temp=%d\n",
                //                        fa_count,i,disperse[i],disperse_temp);

			pp_count++;
			processed ++;  // Move to next pair
			i++;

		//ce[count] = (psize[i]*8.0/( disperse_temp/(double)pp_count));              // compute effective capacity
		disperse_temp=0;
		}
		if (processed >= i_PktNumb) break;
                
	}

		pp_count=1;
	fp=fopen(file_name,"w+");
	fprintf(fp,"ce\n");
	for(i=0;i<count;i++){
		fprintf(fp,"diperse%f\n",ce[i]);
	}
	fclose(fp);

	sort_double(ce, count);
	if( count > 2) {
	median = (ce[count/2]+ce[count/2+1])/2;
	}else{
		median=ce[0];
	}
	

	printf("%d valid tests out %d probes\n\tmedian:%7.2f Mbps\n\tmean: %7.2f Mbps\n\tmin: %7.2f Mbps\n\tmax: %7.2f Mbps\n", 
			count, i_PktNumb, median, mean,ce[0],ce[count-1]);

  fp=fopen(pp_file_name,"w+");
  if ( fp != NULL){
      fprintf(fp,"bseqNo\tseqNo\tpktSize\tdt\n");
      for(i=0;i<i_PktNumb;i++){
          fprintf(fp,"%d\t%d\t%d\t%d\n",
		  bseq[i],
                  seq[i],
                  psize[i],
		  disperse[i]
                  );
      }
  }

  fclose(fp);
  return (median); 
}

//////////////////////////// End PP ///////////////////////////////


//*************************** ProcessPT ***************************//
// Account based on the Myungjin approach
//
double ProcessPT (int i_PktNumb)
{
	int i=0, count=0, expected = 0, loss=0;
	double at[MAX_PKT_NUM], invalidrate, lossrate;
	int  sum = 0;
	double medianAt=0., meanAt=0., meanAtwOverhd=0.;
	double medianAb=0., meanAb=0.;
	int	fa_seq=0;
	int   count_nagg_pkts=0;
	float avgThr=0.0;
	float avgThrwOverhd=0.0;

  for (i=0; i<i_PktNumb; i++) 
    {
      if (seq[i] != expected) // Sequence number is not the one we expected (out of order=loss)
	{
	  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (match 1)! \n", 
		  expected, expected, expected+1, expected); // the expected packet is lost
	  loss++;
	  if (seq[i] > expected) // Bursty loss
	    {
	      expected++;
	      while (seq[i] > expected && expected < i_PktNumb) 
		{
		  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (match 2)! \n", 
			  expected, expected, expected+1, expected); // more losses after the first loss
		  loss++;
		  expected++;
		}
	    }
	}
      
      if (seq[i+1] == seq[i] + 1 && seq[i] == expected) // Good pkts, count the total good ones
	{
//	  disperse[count] = (arrival[i+1].tv_sec - arrival[i].tv_sec) * 1000000 +(arrival[i+1].tv_usec - arrival[i].tv_usec);
	  disperse[count] = (karrival[i+1].tv_sec - karrival[i].tv_sec) * 1000000 + (karrival[i+1].tv_usec - karrival[i].tv_usec);
//	   if ( disperse[count] < MIN_DISPERSION_TIME ){
//		   fa[i]=fa[i+1]=fa_seq;
//	   }
//	   else {
//		   fa_seq++;
//		   fa[i]=fa[i+1]=fa_seq;
//	   }
	  
	  at[count] = (psize[i+1]*8.0/disperse[count]);              // compute achievable throughput
	  sr[count] = psize[i+1]*8.0/(sendtime[i+1]-sendtime[i]);    // compute sending rate
         
	  sum += disperse[count]; 
	  
	  printf("[%2d](%2d-%2d): %d recv in %d usec - At: %7.2f Mbps, sendRate: %7.2f Mbps\n", 
		 expected, expected, expected+1, psize[i+1], disperse[count], at[count], sr[count]);
	  count++;
	}
      else // expected packet is good, however, the next one lost
	{
	  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (next 1)! \n", 
		  expected, expected, expected+1, expected+1); // the next one packet is lost
	  if (expected == i_PktNumb -2) loss++; // Last packet in the train is lost... 
	}
      expected ++;
      if (expected >= i_PktNumb -1) break;
    }

  // in general, one packet loss = two dispersion loss
  // Todo: we can estimate the bursty loss be compare lossrate and invalidrate.
  //
  lossrate = (double)loss/(double)i_PktNumb;                            // loss rate of pkt
  invalidrate = (double)(i_PktNumb - 1 - count)/(double)(i_PktNumb-1);  // loss rate of dispersion

  // Filter out the dispersion time base on the frame aggregation pattern.

//  int aggrThr=FrameAggThr(disperse,count,THRESHOLD_PERCENT);
  printf("**********************************************************\n");
  printf("Threshold for filtering out Aggregated frame is:%d usec\n",aggrThr);
  printf("**********************************************************\n");

	fa_seq=0;
  for ( i=0; i < count; i++){

      if ( disperse[i] < aggrThr ){
          fa[i]=fa[i+1]=fa_seq;
      }
      else {
          fa_seq++;
          fa[i]=fa[i+1]=fa_seq;
      }
  }

  int fa_count=1;
  int k_overhead=0;
  count_nagg_pkts=0;
      for(i=0;i<count;i++){

	  if ( fa[i] == fa[i+1] ){
		  fa_count++;
		  at[i+1]=0;
                  k_overhead+=disperse[i+1];
	  }
	  else{
		 printf("fa_count=%d, disperse[%d]=%d,disperse[%d]=%d, at=[%d]=%7.2f \n",fa_count,i,disperse[i],(i+1),disperse[i+1],i,at[i]);
		 avgThr+=(double)fa_count*psize[i+1]*8/(double)(disperse[i+1]);
		 avgThrwOverhd+=(double)fa_count*psize[i+1]*8/(double)(disperse[i+1]+k_overhead);
		 at[i]=fa_count*psize[i+1]*8/disperse[i+1];
		 printf("fa_count=%d, disperse[%d]=%d,disperse[%d]=%d, at=[%d]=%7.2f,k_overhead= %d \n",fa_count,i,disperse[i],(i+1),disperse[i+1],i,
				 at[i],k_overhead);
		 printf("pksize[%d]=%d * 8 * %d /(%d+%d) = %f, without overhead= %.2f \n",i+1,psize[i+1],fa_count,disperse[i+1],k_overhead,((double)fa_count*psize[i+1]*8/(double)(disperse[i+1]+k_overhead)),((double)fa_count*psize[i+1]*8/(double)(disperse[i+1])) );
		 fa_count=1;
                 k_overhead=0;
		 count_nagg_pkts++;
	  }

      }

  //print raw data into the output
  FILE* fp=fopen(pt_file_name,"w+");
  if ( fp != NULL){
      fprintf(fp,"bseqNo\tseqNo\tpktSize\tsr\tdt\tat\tfa\n");
      for(i=0;i<i_PktNumb;i++){
          fprintf(fp,"%d\t%d\t%d\t%.2f\t%d\t%.2f\t%d\n",
		  bseq[i],
                  seq[i],
                  psize[i],
                  sr[i],
		  disperse[i],
		  at[i],
                  fa[i]
                  );
      }
  }
  fclose(fp);


  printf("Summary of At test with %d valid tests out %d train (%d tests):\n",
         count, i_PktNumb, i_PktNumb - 1);
  printf("\tpacket loss: %d (%f%%) \n\tinvalid result: %d (%f%%)\n",
	 loss, lossrate * 100, i_PktNumb - 1 - count, invalidrate * 100);

  sort_int(disperse, count);
  
  //meanAt = (double)psize[0] * 8.0 / ((double)sum / (double)count);
  //printf("\t Mean At: %f Mbps\n", meanAt);
  meanAt = avgThr/(double)count_nagg_pkts;
  meanAtwOverhd = avgThrwOverhd/(double)count_nagg_pkts;
  
  printf("**************************************************************\n");
  printf("\t New calculation mean At: %f Mbps\n", meanAt);
  printf("\t New calculation mean At with overhead: %f Mbps\n", meanAtwOverhd);
  printf("**************************************************************\n");
  
  //medianAt = (double)psize[0] * 8.0 / (((double)disperse[count/2] + (double)disperse[count/2+1]) / 2.0);
  //printf("\tmean At: %f Mbps\n\tmedian At: %f Mbps\n", meanAt, medianAt);
  //printf("\tmean At: %f Mbps\n", meanAt);
  printf("\t Ab=ce(2-ce/at): %f Mbps\n",allCE * (2.0 -allCE/meanAt));
  printf("\t Ab=ce(2-ce/atwo): %f Mbps\n",allCE * (2.0 -allCE/meanAtwOverhd));

  meanAt=meanAtwOverhd;
  // Equations... need to play around to compare the performance.
  // And to return At if the At is less than half Ce
 // if (meanAt >= allCE ) 
  if (meanAt <= allCE/2 ) 
    {
       meanAb = meanAt;
    }
  else
    {
       meanAb = allCE * (2.0 - allCE/meanAt);
    }
  
  /*
  printf("\tmean Ab: %f Mbps\n\tmedian Ab: %f Mbps\n", meanAb, medianAb);
  printf("\tmean Ab with loss: %f Mbps\n\tmedian Ab with loss: %f Mbps\n", meanAb*(1.0-lossrate), medianAb*(1.0-lossrate));
  */

  printf("\tmean Ab: %f Mbps\n", meanAb);
  printf("\tmean Ab with loss: %f Mbps\n", meanAb*(1.0-lossrate));
 // printf("\tmedian Ab: %f Mbps\n", medianAb);
 // printf("\tmedian Ab with loss: %f Mbps\n", medianAb*(1.0-lossrate));
  
  

  if (meanAb < 0) 
    {
      return 0; 
    }
  else 
    {
      return meanAb*(1.0-lossrate);
    }
      
} // 

// Account based on the Arsham's approach
double ProcessPTdr (int i_PktNumb)
{
//  int i=0, count=0, expected = 0, loss=0;
//  double at[MAX_PKT_NUM], invalidrate, lossrate;
//  int sum = 0;
//  double medianAt=0., meanAt=0.;
//  double medianAb=0., meanAb=0.;
//  int	fa_seq=0;
//
//  for (i=0; i<i_PktNumb; i++) 
//    {
//      if (seq[i] != expected) // Sequence number is not the one we expected (out of order=loss)
//	{
//	  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (match 1)! \n", 
//		  expected, expected, expected+1, expected); // the expected packet is lost
//	  loss++;
//	  if (seq[i] > expected) // Bursty loss
//	    {
//	      expected++;
//	      while (seq[i] > expected && expected < i_PktNumb) 
//		{
//		  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (match 2)! \n", 
//			  expected, expected, expected+1, expected); // more losses after the first loss
//		  loss++;
//		  expected++;
//		}
//	    }
//	}
//      
//      if (seq[i+1] == seq[i] + 1 && seq[i] == expected) // Good pkts, count the total good ones
//	{
//	 // disperse[count] = (arrival[i+1].tv_sec - arrival[i].tv_sec) * 1000000 + (arrival[i+1].tv_usec - arrival[i].tv_usec);
//	  disperse[count] = (karrival[i+1].tv_sec - karrival[i].tv_sec) * 1000000 + (karrival[i+1].tv_usec - karrival[i].tv_usec);
//	  at[count] = (psize[i+1]*8.0/disperse[count]);              // compute achievable throughput
//	  sr[count] = psize[i+1]*8.0/(sendtime[i+1]-sendtime[i]);    // compute sending rate
//         
//	  //sum += disperse[count]; 
//	  
//	  printf("[%2d](%2d-%2d): %d recv in %d usec - At: %7.2f Mbps, sendRate: %7.2f Mbps\n", 
//		 expected, expected, expected+1, psize[i+1], disperse[count], at[count], sr[count]);
//	  count++;
//	}
//      else // expected packet is good, however, the next one lost
//	{
//	  printf ("[%2d](%2d-%2d): Dispersion invalid due to packet #%d lost (next 1)! \n", 
//		  expected, expected, expected+1, expected+1); // the next one packet is lost
//	  if (expected == i_PktNumb -2) loss++; // Last packet in the train is lost... 
//	}
//      expected ++;
//      if (expected >= i_PktNumb -1) break;
//    }
//
//  // in general, one packet loss = two dispersion loss
//  // Todo: we can estimate the bursty loss be compare lossrate and invalidrate.
//  //
//  lossrate = (double)loss/(double)i_PktNumb;                            // loss rate of pkt
//  invalidrate = (double)(i_PktNumb - 1 - count)/(double)(i_PktNumb-1);  // loss rate of dispersion
//
//  // Filter out the dispersion time base on the frame aggregation pattern.
//
////aggrThr=FrameAggThr(disperse,count,THRESHOLD_PERCENT);
//printf("**********************************************************\n");
//printf("Threshold for filtering out Aggregated frame is:%d usec\n",aggrThr);
//printf("**********************************************************\n");
//
//for ( i=0; i < count; i++){
//
//	   if ( disperse[i] < aggrThr ){
//		   fa[i]=fa[i+1]=fa_seq;
//	   }
//	   else {
//		   fa_seq++;
//		   fa[i]=fa[i+1]=fa_seq;
//	   }
//}
//	  
//  int fa_count=1;
//  int avg_dr=0; //to store small dispersion time for the frame aggregation.
//  int count_nagg_pkts=0;
//      for(i=0;i<i_PktNumb-1;i++){
//
//	  if ( fa[i] == fa[i+1] ){
//		  avg_dr+=( (disperse[i] <= disperse[i+1])?disperse[i]:disperse[i+1] );
//		  fa_count++;
//	  }
//	  else{
//		 printf("fa_count=%d, disperse[%d]=%d,disperse[%d]=%d\n",fa_count,i,disperse[i],(i+1),disperse[i+1]);
//		 disperse[i+1]=(disperse[i+1]+avg_dr)/fa_count;
//		 printf("fa_count=%d, disperse[%d]=%d,disperse[%d]=%d\n",fa_count,i,disperse[i],(i+1),disperse[i+1]);
//		 fa_count=1; 
//		 avg_dr=0;
//		 sum+=disperse[i];
//		 count_nagg_pkts++;
//	  }
//
//      }
//
//  //print raw data into the output
//  FILE* fp=fopen(pt_file_name,"w+");
//  if ( fp != NULL){
//      fprintf(fp,"bseqNo\tseqNo\tpktSize\tsr\tdt\tat\tfa\n");
//      for(i=0;i<i_PktNumb;i++){
//          fprintf(fp,"%d\t%d\t%d\t%.2f\t%d\t%.2f\t%d\n",
//		  bseq[i],
//                  seq[i],
//                  psize[i],
//                  sr[i],
//		  disperse[i],
//		  at[i],
//                  fa[i]
//                  );
//      }
//  }
//  fclose(fp);
//
//
//  printf("Summary of At test with %d valid tests out %d train (%d tests):\n",
//         count, i_PktNumb, i_PktNumb - 1);
//  printf("\tpacket loss: %d (%f%%) \n\tinvalid result: %d (%f%%)\n",
//	 loss, lossrate * 100, i_PktNumb - 1 - count, invalidrate * 100);
//
//  sort_int(disperse, count);
//  
//  meanAt = (double)psize[0] * 8.0 / ((double)sum / (double)count_nagg_pkts);
//  medianAt = (double)psize[0] * 8.0 / (((double)disperse[count/2] + (double)disperse[count/2+1]) / 2.0);
//  //printf("\tmean At: %f Mbps\n\tmedian At: %f Mbps\n", meanAt, medianAt);
//  printf("\tmean At: %f Mbps\n", meanAt);
//  printf("\t Ab=ce(2-ce/at): %f Mbps\n",allCE * (2.0 -allCE/meanAt));
//
//  // Equations... need to play around to compare the performance.
//  // And to return At if the At is less than half Ce
//  if (meanAt >= allCE ) 
//    {
//       meanAb = meanAt;
//    }
//  else
//    {
//      meanAb = allCE * (2.0 - allCE/meanAt);
//    }
//
//  
//  if (medianAt >= allCE )
//    {
//       medianAb = medianAt;
//    }
//  else
//    {
//      medianAb = allCE * (2.0 - allCE/medianAt);
//    }
//  /*
//  printf("\tmean Ab: %f Mbps\n\tmedian Ab: %f Mbps\n", meanAb, medianAb);
//  printf("\tmean Ab with loss: %f Mbps\n\tmedian Ab with loss: %f Mbps\n", meanAb*(1.0-lossrate), medianAb*(1.0-lossrate));
//  */
//
//  printf("\tmean Ab: %f Mbps\n", meanAb);
//  printf("\tmean Ab with loss: %f Mbps\n", meanAb*(1.0-lossrate));
//  printf("\tmedian Ab: %f Mbps\n", medianAb);
//  printf("\tmedian Ab with loss: %f Mbps\n", medianAb*(1.0-lossrate));
//  
//  
//
//  if (meanAb < 0) 
//    {
//      return 0; 
//    }
//  else 
//    {
//      return meanAb*(1.0-lossrate);
//    }
//      
} // 
/////////////////////////////////////////////////////////////////////////////
void InitStorage()
{
  struct timeval notime;
  int i = 0;
  notime.tv_sec = 0;
  notime.tv_usec = 0;

  for (i=0; i<MAX_PKT_NUM; i++)
    {
      seq[i] = -1;
      bseq[i]=-1;
      fa[i]=0;
      sendtime[i] = -1;
      psize[i]= -1;
      arrival[i] = notime;
      karrival[i] = notime;
      disperse[i] = -1;
      ce[i] = -1;
      ceflag[i] = -1;
    }
} // end InitStorage()

/////////////////////////////////////////////////////////////////
void sort_double (double arr[], int num_elems)
{
  int i,j;
  double temp;

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
} // end sort_double()

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

/////////////////////////////////////////////////////////////////////////////
void CleanUp(int arg1) 
{
  close (tcpSocket);
  close (udpSocket);
  printf("WBest receiver is now off\n");
  exit(0);
} //end CleanUp()
//******************************************************************
// calculate the optimal threshold for dispersion time 
// input is the dispersion array 
// output is the 
// *****************************************************************
int FrameAggThr(int dt[],int num_elems,int perc){
    FILE* fp=fopen(dthist_file_name,"w+");
    const int bins[]={0,50,100,200,300,400,500,600,700,800,900,1000,10000}; // bins for dispersion histogram
    
    int freq[13];
    memset(freq,0,sizeof(freq));
    int dt_temp[MAX_PKT_NUM];    
    memcpy(dt_temp,dt,sizeof(dt_temp));
    printf("******************FrameAggrThr*****************\n");
    sort_int(dt_temp,num_elems);
    int sum=0,count=0;
    int i=0; 
    int perc_index= (int)(num_elems*perc/100.);
    for (i=perc_index;i< num_elems;i++){
        sum+=dt_temp[i];
        count++;
    }
    // find the frequency of each dispersion time
    int j=0;
    for (i=0;i<num_elems;i++){
        for(j=0;j<12;j++){
        if(bins[j] <= dt[i] && dt[i] < bins[j+1])
            freq[j]++;
       }
    }
    int max=bins[2],secondmax=bins[1];
    for(j=0;j<12;j++){
    
        if (freq[j]>1){
        secondmax=max;
        max=bins[j];
    }

    }

    //print them out
    for (j=0;j<12;j++){
    printf("[%d,%d):%d\n",bins[j],bins[j+1],freq[j]);
    fprintf(fp,"[%d,%d)\t%d\n",bins[j],bins[j+1],freq[j]);
    //find the maximum
    }
fclose(fp);

    return(( (freq[11]+freq[10]+freq[9])>3 )?(int)(max+secondmax)/2:(int)(bins[1]+bins[7])/2);
    //return ((int)(sum/count));
}


double ProcessPP (int i_PktNumb)
{
//  // Show that we've received some data
//  int processed = 0, count=0, i=0;
//  double sum = 0., sum_disperse=0., mean = 0., median=0.,kmedian=0.,kmean=0.,sum_kdisperse=0.;
//  FILE *fp;
//	
//  for (i=0; i<i_PktNumb*2-1; i++) 
//    {
//	//printf("bseq[%d]=%d,seq[%d]=%d\n",i,bseq[i],i,seq[i]);
//      if (seq[i] == seq[i+1] && seq[i] >= 0 ) //packet pair not lost
//	{
//	  if ( processed > seq[i]) continue; // duplicated packet
//	  while (processed < seq[i])        // we skip any pairs?
//	    {
//	      ceflag[processed] = 0;                               // this pair is not valid
//	      printf("[%2d]: Packet pair lost\n", processed);
//	      processed ++;
//	    }
//
//    	  ceflag[processed] = 1;                                     // this pair is valid
//
//	  disperse[count] = (arrival[i+1].tv_sec - arrival[i].tv_sec) * 1000000 +
//	    (arrival[i+1].tv_usec - arrival[i].tv_usec);
//
//	  kdisperse[count] = (karrival[i+1].tv_sec - karrival[i].tv_sec) * 1000000 +
//	    (karrival[i+1].tv_usec - karrival[i].tv_usec);
//
//	  ce[count] = (psize[i+1]*8.0/disperse[count]);              // compute effective capacity
//	  kce[count] = (psize[i+1]*8.0/kdisperse[count]);              // compute effective capacity
//	  sr[count] = psize[i+1]*8.0/(sendtime[i+1]-sendtime[i]);    // compute sending rate
//	  sum_disperse+=disperse[count];
//	  if (ce[count] > 0.) 
//	    {
//	      sum += ce[count];
//	      printf("%2d, %d recv in %d usec, kdr %d usec, Ce: %7.2f Mbps, KCe: %7.2f, sendRate: %7.2f Mbps\n", seq[i+1],psize[i+1],disperse[count],kdisperse[count], ce[count],kce[count], sr[count]);
//
//	      count ++ ; // increase valid packet pair by 1
//	    }
//
//
//	  processed ++;  // Move to next pair
//
//	  if (processed >= i_PktNumb) break;
//	}
//    }
//  
//  mean = sum/count;
//
//  fp=fopen(file_name,"w+");
//    
//  fprintf(fp,"burstSize,disperse,ce\n");
//  for(i=0;i<count;i++){
//   fprintf(fp,"%d,%d,%f\n",burstSize,disperse[i],ce[i]);
//  }
//  fclose(fp);
//  
//  sort_double(ce, count);
//  sort_double(kce,count);
//  sort_double(disperse, count);
//  sort_double(kdisperse, count);
//
//  median = (ce[count/2]+ce[count/2+1])/2;
//  kmedian = (kce[count/2]+kce[count/2+1])/2;
//  
//
//  printf("Summary of Ce test with %d valid tests out %d pairs:\n\tmedian: %f Mbps\n\tmean: %f Mbps\n\tmin: %f Mbps\n\tmax: %f Mbps\n", 
//	 count, i_PktNumb, median, mean,ce[0],ce[count-1]);
//  printf("Summary of Kernel timestamp Ce test with %d valid tests out %d pairs:\n\tmedian: %f Mbps\n\tmean: %f Mbps\n\tmin: %f Mbps\n\tmax: %f Mbps\n", 
//	 count, i_PktNumb, kmedian, kmean,kce[0],kce[count-1]);
//
//  printf("\tmedian\tmean\tmin\tmax\n"); 
//  printf("ce:\t%f\t%f\t%f\t%f\n",median, mean,ce[0],ce[count-1]);
//  printf("disperse:\t\t%f\t\t%f\t\t%d\t\t%d\n",
//          (disperse[count/2]+disperse[count/2+1])/2, 
//          (sum_disperse/count),
//          disperse[0],
//          disperse[count-1]
//          );
//  if (effective_capacity){
//
//      printf("********************************************\n");
//      printf("Effective Capacity is chosen as specified in the command prompt%d\n",effective_capacity);
//      return (effective_capacity);
//	
//}
//
//  if (median >= 0) 
//    {
//      return median; 
//    }
//  else
//    {
//      return 0.0;
//    }
} // end ProcessPP()
