# Makefile: the Makefile of WBest (V1.0) for Linux.
# 
# $Id: Makefile,v 1.5 2006/05/12 19:08:20 lmz Exp $
# 
# Author:  Mingzhe Li (lmz@cs.wpi.edu)
#          Computer Science Dept.
#          Worcester Polytechnic Institute (WPI)
#
# WBEST+ Author: Arsham Farshad (arsham.farshad@gmail.com)
#        Informatics Dept., Edinburgh University, Scotland
# History
#   Version 1.0 - 2006-05-10
# 
# Description:
#       Makefile
#              Makefile of WBest, which is a bandwidth estimation tool
#              designed for wireless streaming application.
#              Wireless Bandwidth ESTimation (WBest)
# 
# License:
#   This software is released into the public domain.
#   You are free to use it in any way you like.
#   This software is provided "as is" with no expressed
#   or implied warranty.  I accept no liability for any
#   damage or loss of business that this software may cause.
###################################################################

CC=gcc

CFLAGS=-g -Wall -O2
CPPFLAGS=
LIBS=-lm 
LDFLAGS=

SOBJS=wbest_snd.o
ROBJS=wbest_rcv.o 
OBJS=$(SOBJS) $(ROBJS)

TARGETS=wbest_snd wbest_rcv

all:${TARGETS} clean_obj

wbest_snd: $(SOBJS)
	$(CC) $(SOBJS) -o wbest_snd $(LIBS) $(LDFLAGS) $(CFLAGS)

wbest_rcv: $(ROBJS)
	$(CC) $(ROBJS) -o wbest_rcv $(LIBS) $(LDFLAGS) $(CFLAGS)

clean: 
	rm -f ${OBJS} ${TARGETS} *~

clean_obj:
	rm -f ${OBJS} *~

tags:
	find . -name "*.[ch]*" -print | etags -


