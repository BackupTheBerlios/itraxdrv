/*
    testtracker.cc  

    InterSense InterTrax 2 USB Tracker (Library) Testprogram

    Jan-Friso Evers evers@mip.informatik.uni-kiel.de
    MIP Kiel

    2000/11/30
  
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 
*/ 



#include <unistd.h>                                                           
#include <stdio.h>
#include <fcntl.h>
#include <curses.h>
#include <sys/poll.h>



#include "itrax.h"

#define DEVICENAME "/dev/input/tracker0"


/*  #define POLL  */



int main (int argc,char *argv[])
{
   int tracker;
   struct trackerposition data;
#ifdef POLL
   struct pollfd ufds;
#endif

   /* store results from ioctl() here: */
   short id[4];
   char name[100];
   int abs[5];
   float call_data[3]; 
  int finished,callibrated;
  struct filtercontrol  filter;
  char c;
  WINDOW *win;
  float coeff[]={0.2 , 0.2 , 0.2 , 0.2 , 0.2};


  if ( (tracker = open(DEVICENAME,O_NONBLOCK)) < 0) 
    {
      perror(DEVICENAME);
      exit(1);
    }


  if (ioctl(tracker,ITRAXIOCGID,&id)<0) {
    perror("ioctl ITRAXIOCGID");
    exit(1);
  }   
  printf("Bus: %d Vendor:%d Product:%d Version:%d\n",id[0],id[1],id[2],id[3]);
  

  if (ioctl(tracker,ITRAXIOCGNAME(100),&name)<0) {
    perror("ioctl ITRAXIOCGNAME");
    exit(1);
  }   
  printf("Name: %s\n",name);

 if (ioctl(tracker,ITRAXIOCGABS(5),&abs)<0) {
    perror("ioctl ITRAXIOCGABS");
    exit(1);
  }   
 printf("abs0:%d abs1:%d abs2:%d abs3:%d abs4:%d\n",abs[0],abs[1],abs[2],abs[3],abs[4]);


 filter.type = ITRAX_FILTER_FASTMEAN;
 filter.size = 5;
 filter.axes = 0;
 filter.coeff = coeff;

 if (ioctl(tracker,ITRAXIOCSFILTER,&filter ) <0) 
	    perror("ioctl ITRAXIOCSFILTER");
 if (ioctl(tracker,ITRAXIOCGFILTER,&filter) <0) {
   perror("ioctl ITRAXIOCGFILTER");
   exit(1);
 }
 printf("filter axes: %d type: %d size: %d\n",filter.axes,
	filter.type,filter.size);

 

#ifdef POLL
 ufds.fd = tracker;
 ufds.events = POLLIN ;
 printf("enter poll()\n");
 poll(&ufds,1,-1);
 printf("poll returned POLLIN \n");
#endif
 sleep(1);
 
  // ncurses needs this 
  win = initscr();
  cbreak();
  noecho();
  nodelay(win,TRUE);


  
  finished = FALSE;
  callibrated = FALSE;

  while ( ! finished  ) 
    { 
 
      read(tracker, &data, sizeof (struct trackerposition));
      if (callibrated)
	fprintf(stderr," %5.1f %5.1f %5.1f C\r",data.callibrate[0],data.callibrate[1],
		data.callibrate[2]);   
      else
	fprintf(stderr," %5.1f %5.1f %5.1f  \r",data.raw[0],data.raw[1],data.raw[2]);
      
      c = getch();
      switch (c) 
	{
	case 'q': 
	  finished = TRUE;
	  break;
	case 'c':  
	  if (ioctl(tracker,ITRAXIOCSCALL,0) <0) {
	    perror("ioctl ITRAXIOCSCALL");
	    exit(1);
	  }   
	  callibrated = TRUE;
	  break;
	case 'u':  
	  call_data[0]=10.0;
	  call_data[1]=20.0;
	  call_data[2]=30.0;
	  if (ioctl(tracker,ITRAXIOCSCALLUSER,call_data) <0) {
	    perror("ioctl ITRAXIOCSCALLUSER");
	    exit(1);
	  }   
	  callibrated = TRUE;
	  break;
	case 'd':
	  callibrated = FALSE;
	  break;
	default:
	  break;
	}
    }
  echo();
  nocbreak();
  endwin();  
	  call_data[0]=0.0;
	  call_data[1]=0.0;
	  call_data[2]=0.0;


	  if (ioctl(tracker,ITRAXIOCGCALL,call_data) <0)
	    perror("ioctl ITRAXIOCGCALL");
	  else
	    printf("Call: %f %f %f\n",call_data[0],call_data[1],call_data[2]);


  
  close(tracker);
  return 0;
};



