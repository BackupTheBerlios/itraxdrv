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


/* #define POLL */
#define USE_CURSES


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
   int finished;
/*    struct filtercontrol  filter; */
  char c;
  WINDOW *win;
/*    int coeff[]={2 , 2 , 2 , 2 , 2}; */


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


/*   filter.type = ITRAX_FILTER_FASTMEAN; */
/*   filter.size = 5; */
/*   filter.axes = 0; */
/*   filter.coeff = coeff; */

/*   if (ioctl(tracker,ITRAXIOCSFILTER,&filter ) <0)  */
/*  	    perror("ioctl ITRAXIOCSFILTER"); */
/*   if (ioctl(tracker,ITRAXIOCGFILTER,&filter) <0) { */
/*     perror("ioctl ITRAXIOCGFILTER"); */
/*     exit(1); */
/*   } */
/*   printf("filter axes: %d type: %d size: %d\n",filter.axes, */
/*  	filter.type,filter.size); */

 

#ifdef POLL
 ufds.fd = tracker;
 ufds.events = POLLIN ;
#endif

 
  // ncurses needs this 
#ifdef USE_CURSES
  win = initscr();
  cbreak();
  noecho();
  nodelay(win,TRUE);
#endif

  
  finished = FALSE;

  while ( ! finished  ) 
    { 
#ifdef POLL      
      poll(&ufds,1,1); 
#endif
      read(tracker, &data, sizeof (struct trackerposition));

#ifdef USE_CURSES
      printf(" \r%5.1f %5.1f %5.1f  ",data.tenthdegree[0]/10.0,data.tenthdegree[1]/10.0,data.tenthdegree[2]/10.0 );
#else
      printf(" %5.1f %5.1f %5.1f  \n",data.tenthdegree[0]/10.0,data.tenthdegree[1]/10.0,data.tenthdegree[2]/10.0 );
#endif

      
#ifdef USE_CURSES
      c = getch();
      switch (c) 
	{
	case 'q': 
	  finished = TRUE;
	  break;
	default:
	  break;
	}
#endif
    }

#ifdef USE_CURSES
  echo();
  nocbreak();
  endwin();  
#endif

  close(tracker);
  return 0;
};



























