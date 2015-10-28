# WBest+
WBest+ is a tool for active bandwidth measurement of paths have a WiFi communication link based on the IEEE802.11n/ac Standard. It is based on the previous tool named WBest. WBest+ is developed in the Edinburgh University, School of Informatics and used for the experiments in the paper titled "On the Impact of 802.11n Frame Aggregation on End-to-End Available Bandwidth Estimation", SECON2014. 
The original WBest tool was developed by Mingzhe Li ,  Computer Science Dept., Worcester Polytechnic Institute (WPI).
# Parameters for tunning the Wbest+ based on your network WiFi setup.

int i_PktSize     = 1460;                    // The packet size in bytes.

int i_PktNumbPP   = 165;                      // number of  thepacket pairs.

int i_PktNumbBPP  = 33;                       // number of packet pairs that are sent together without pausing for pp_pausetime.

int i_PktNumbPT   = 100;                      // length of the packet train.

int i_rate = 0;   // For debugging, if it is given as a parameter in the commandline, it overwrite the PP estimated capacity and used as a rate for transmitting the packets in the packet train.

int pp_pausetime=10000; // pause time between packet pairs transmission the default value is 10msec.

#How to run the code:

Run the Receiver: 

./wbest_rcv -f [Results Logfile] -d 0

Run the Sender:

./wbest_snd -h [Destination Address] -m [packet train lenght]

