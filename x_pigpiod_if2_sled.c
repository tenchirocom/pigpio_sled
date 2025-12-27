/*
gcc -I. -L. -Wall -pthread -o x_pigpiod_if2_sled x_pigpiod_if2_sled.c -lpigpiod_if2
./x_pigpiod_if2

*** WARNING ************************************************
*                                                          *
* All the tests make extensive use of gpio 25 (pin 22).    *
* Ensure that either nothing or just a LED is connected to *
* gpio 25 before running any of the tests.                 *
*                                                          *
* Some tests are statistical in nature and so may on       *
* occasion fail.  Repeated failures on the same test or    *
* many failures in a group of tests indicate a problem.    *
************************************************************
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "pigpiod_if2.h"

int main(int argc, char *argv[])
{
   int pi;

   pi = pigpio_start(0, 0);

   if (pi < 0)
   {
      fprintf(stderr, "pigpio initialisation failed (%d).\n", pi);
      return 1;
   }

   printf("Connected to pigpio daemon (%d).\n", pi);

   printf("Start testing strip led interfaces:\n");
   printf("---> Channel configuration...\n");
   sled_channel(pi, 64, 18, 0, 0);
   printf("---> Begin...\n");
   sled_begin(pi);
   printf("---> Set led 0 RED\n");
   sled_set(pi, 0, 0xFF0000, 0);
   printf("---> Set led 0 BLUE channel 1\n");
   sled_set(pi, 0, 0x0000FF, 1);
   printf("---> Set led 3 GREEN\n");
   sled_set(pi, 3, 0x00FF00, 0xFF);
   printf("---> Set led 6 GREEN\n");
   sled_set(pi, 6, 0x0000FF, 0);
   sled_render(pi);

   for (int j = 0; j < 5; j++)
   for (int i = 0; i <= 7; i++) {
      if (i==0||i==3||i==6) i++;

      sled_set(pi, i, 0xFF0000, 0);
      sled_render(pi);
      sled_set(pi, i, 0, 0);
      sleep(1);
   }
   sled_render(pi);
   
   sled_end(pi);

   pigpio_stop(pi);

   return 0;
}

