/*
 * JTAG pinout detector for Raspberry Pi
 * Version 0.1
 * 11/27/2015
 *
 * Copyright (c) 2015 Michael Bazzinotti (Bazz)
 * Copyright (c) 2010 Igor Skochinsky
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
*/


/*

 The overall idea:
 1) choose any 2 pins as TMS and TCK
 2) using them, reset TAP and then shift DR, while observing the state of all other pins
 3) for every pin that received anything like a TAP ID (i.e. bit 0 is 1):
 4)   using the pin as TDI, feed in a known pattern into the TAP
 5)   keep shifting and monitor the rest of the pins
 6)   if any of the remaining pins received the pattern, it is TDO
 
 Current algorithm assumes chain length of 1
 
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <wiringPi.h>
#include <string.h>

void print_usage(char **argv)
{
    printf("usage\n");
    printf("%s [-n #pins]\n", argv[0]);
}

struct DigitalInOut
{
  DigitalInOut()
  {
    pin = count++;
  }

  int read()
  {
    return digitalRead(pin);
  }

  void write(int val)
  {
    digitalWrite(pin, val);
  }

  void output()
  {
    pinMode(pin, OUTPUT);
  }

  void input()
  {
    pinMode(pin, INPUT);
  }

  void pullUp()
  {
    pullUpDnControl(pin, PUD_UP);
  }
  void pullDown()
  {
    pullUpDnControl(pin, PUD_DOWN);
  }
  void pullOff()
  {
    pullUpDnControl(pin, PUD_OFF);
  }

  int pin;

  static int count;
};

int DigitalInOut::count=0;

// pin description structure
struct PinInfo
{
  DigitalInOut pin;
  char *name;
};

// define the pins to use for bruteforcing
PinInfo *g_pins;    // populated in main()
int NPINS = 0;      // defined via command line

// indexes of found pins
int pin_TCK = -1;
int pin_TMS = -1;
int pin_TDO = -1;
int pin_TDI = -1;

inline const char *pinname(int pinno)
{
  return pinno == -1 ? "unknown" : g_pins[pinno].name;
}

// print current layout
inline void printpins()
{
  printf("TCK: %s, TMS: %s, TDO: %s, TDI: %s\n",
    pinname(pin_TCK), pinname(pin_TMS), pinname(pin_TDO), pinname(pin_TDI));
}

// set pin with a given index
inline void setpin(int pinno, int value)
{
  g_pins[pinno].pin.write(value != 0);
  delayMicroseconds(500);
}

// read pin with a given index
inline int getpin(int pinno)
{
  return g_pins[pinno].pin.read();
}

// make a pin output pin
inline void makeoutput(int pinno)
{
  g_pins[pinno].pin.output();
  g_pins[pinno].pin.pullOff();
}

// make a pin input pin
inline void makeinput(int pinno)
{
  g_pins[pinno].pin.input();
  g_pins[pinno].pin.pullUp();
}

// init all pins as inputs
void InitGPIO()
{
  wiringPiSetup();
  for (int i=0; i<NPINS; i++)
    makeinput(i);
}

// feed in one tms pulse with given value
void clock_tms(int tms)
{
  //TMS is sampled by TAP on the rising edge of TCK, i.e. on change from 0 to 1
  //so the proper sequence is TCK=0; TMS=value; TCK=1
  setpin(pin_TCK, 0);
  setpin(pin_TMS, tms);
  //printpins(1);
  //Delay(1);
  setpin(pin_TCK, 1);
  //printpins(1);
  setpin(pin_TCK, 0);//to make sure the TDO value gets set
}

// same as above but also set the tdi and return tdo
int clock_tms_tdi(int tms, int tdi)
{
  setpin(pin_TCK, 0);
  setpin(pin_TMS, tms);
  setpin(pin_TDI, tdi);
  //printpins(2);
  //Delay(1);
  setpin(pin_TCK, 1);
  //printpins(2);
  setpin(pin_TCK, 0);//to make sure the TDO value gets set
  return getpin(pin_TDO);
}

// the end
void exit_on_button(char* msg)
{
  printf(msg);
  while(1);
}

const unsigned int pattern=0xAA55AA55;

void try_tdi(int tdi)
{
  int output;
  if (tdi==pin_TCK || tdi==pin_TMS || tdi==pin_TDO)
    return;  
  //at this point we know which pin is TDO
  //so we'll shift a known pattern into all possible TDIs and monitor the TDO
  //since we were shifting in garbage into them during reading the IDCODE, 
  //we'll skip the 32 bits of output while we're shifting in the pattern
  pin_TDI = tdi;
  printf("Trying %s as TDI\n", pinname(tdi));
  makeoutput(tdi);

  printpins();
  //reset TAP (11111)
  puts("Resetting TAP\n");
  clock_tms(1);clock_tms(1);clock_tms(1);clock_tms(1);clock_tms(1);
  //go to Shift DR (0100)
  puts("Go to Shift DR\n");
  clock_tms(0);clock_tms(1);clock_tms(0);clock_tms(0);
  puts("Shifting in pattern.\n");
  
  for (int i=0; i<32; i++)
  {
    //printf("%d: ",i);
    clock_tms_tdi(0, pattern & (1<<i));
    //puts(stringbuf);putc('\r');putc('\n');        
  }
  printf("Reading in the output.\n");
  //now keep shifting and read output from TDO
  output = 0;
  for (int i=0;i<32;i++)
  {
    //printf("%d: ",i);
    output |= getpin(pin_TDO) << i;
    clock_tms_tdi(0, 0);
    //puts(stringbuf);putc('\r');putc('\n');    
  }
  printf("Got %08X (expected %08X)\n", output, pattern);
  if (output==pattern)
  {
    printf("Success! Final pinout:\n");
    printpins();
    //exit_on_button("Found!");
    exit(0);
  }
  else if (output!=0 && output!=0xFFFFFFFF)
  {
    printf("Got some pattern but not ours...");
  }
  makeinput(tdi);
}

union idcode_reg {
  struct {
    unsigned int res: 1;
    unsigned int manuf_id: 11;
    unsigned int part_no: 16;
    unsigned int version: 4;
  };
  unsigned int reg32;
};

// check if we can see a valid TAP id on any of the pins
void try_id()
{
  int outputs[NPINS] = {0};
  printf("Trying TCK %s, TMS %s\n", pinname(pin_TCK), pinname(pin_TMS));
  makeoutput(pin_TCK);
  makeoutput(pin_TMS);
  printpins();
  //reset TAP (11111)
  printf("Resetting TAP\n");
  clock_tms(1);clock_tms(1);clock_tms(1);clock_tms(1);clock_tms(1);
  //go to Shift DR (0100)
  printf("Go to Shift DR\n");
  clock_tms(0);clock_tms(1);clock_tms(0);clock_tms(0);
  //shift out the DR
  printf("Shifting DR\n");
  //printpins(1);
  //puts(stringbuf);putc('\r');putc('\n');
  for (int i=0;i<32;i++)
  {
    //printf("%d: ",i);
    for (int j=0;j<NPINS;j++)
    {
      if (j!=pin_TCK && j!=pin_TMS)
        outputs[j] |= getpin(j) << i;
    }
    clock_tms(0);
    //puts(stringbuf);putc('\r');putc('\n');
    //printpins(1);
  }
  puts("Done. Checking pin outputs.\n");
  bool found = false;
  for (int j=0;j<NPINS;j++)
  {
    if (j==pin_TCK || j==pin_TMS) 
      continue;
    //printf("%2d: %08X\n", j, outputs[j]);    
    //check for possible ID code: 32 bits captured are not all zeroes or ones and the 0th bit is 1
    if (outputs[j]!=0 && outputs[j]!=0xFFFFFFFF && (outputs[j]&1) )
    {
      found = true;
      union idcode_reg idc;
      idc.reg32 = outputs[j];
      printf("Found a possible TDO on %s. ID code reply is %08X (manufacturer: %02X, part no.: %02X, version: %X)\n",
        pinname(j), outputs[j], idc.manuf_id, idc.part_no, idc.version);
      pin_TDO = j;
      for (int k=0;k<NPINS;k++)
        try_tdi(k);
    }
  }
  if ( !found )
    printf("Didn't find any good reply...\n");
  makeinput(pin_TCK);
  makeinput(pin_TMS);
}

// try the selected pins
void try_comb(int v[], int maxk)
{
    //for (int i=0; i<maxk; i++) printf ("%i ", v[i]);
    //printf ("\n");
    pin_TCK = v[0];
    pin_TMS = v[1];
    try_id();
    pin_TCK = v[1];
    pin_TMS = v[0];
    try_id();
}

// source: http://userweb.cs.utexas.edu/users/djimenez/utsa/cs3343/lecture25.html
void combinations(int v[], int start, int n, int k, int maxk)
{
    int     i;
    /* k here counts through positions in the maxk-element v.
     * if k >= maxk, then the v is complete and we can use it.
     */
    if ( k >= maxk )
    {
        try_comb(v, maxk);
        return;
    }

    /* for this k'th element of the v, try all start..n
     * elements in that position
     */
    for (i=start; i<n; i++)
    {
        v[k] = i;
        /* recursively generate combinations of integers
         * from i+1..n
         */
        combinations (v, i+1, n, k+1, maxk);
    }
}

int main(int argc, char **argv)
{
    int c, index;
    opterr = 0;
    while ((c = getopt (argc, argv, "n:")) != -1)
        switch (c)
        {
            case 'n':
                NPINS = atoi(optarg);
                break;
            case '?':
                if (optopt == 'c')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return 1;
            default:
                abort ();
        }

    for (index = optind; index < argc; index++)
        printf ("Non-option argument %s\n", argv[index]);

    if (NPINS == 0)
    {
        print_usage(argv);
        exit(1);
    }

    printf ("NPINS = %d\n", NPINS);

    g_pins = (PinInfo *) malloc(sizeof(PinInfo) * NPINS);

    for (int i=0; i < NPINS; i++)
    {
        char gpio_str[25];
        sprintf(gpio_str, "gpio %d", i);
        char *str = (char *) malloc ((strlen(gpio_str) + 1) * sizeof(char));
        strcpy(str, gpio_str);
        g_pins[i] = {DigitalInOut(), str};

        printf("%s\n", str);
    }

    int *v = (int *) malloc (sizeof(int) * NPINS);

    printf ("count == %d\n", DigitalInOut::count);
    InitGPIO();

    // try all combinations of 2 pins
    combinations (v, 0, NPINS, 0, 2);

    for (int i=0; i < NPINS; i++)
    {
        free(g_pins[i].name);
    }
    free(v);
}
