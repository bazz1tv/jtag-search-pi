== jtag-find for Raspberry Pi ==

This is an adaption of Igor Skochinsky's JTAG_Search for mbed to the Raspberry Pi, with additional improvement. Like Igor's, it will only identify TDI, TMS, TCK, TDO. It does not identify certain JTAG pins -- nTRST, RTCK.

== How to Compile == 
wiringPi library must be installed. This can easily be installed from Raspbian: `sudo apt-get install wiringpi`

== How to Use ==

It uses wiringPi's GPIO numbering system, see http://wiringpi.com/pins/ for more details.

You must connect your pins starting from wiringPi Pin 0. Make your subsequent connections along with the numbering scheme.

When you're ready to run the application. As an example, let's say you're checking 9 pins:

`sudo ./jtag-find -n 9`

It's that simple.

There's a default pre-programmed delay of half-millisecond from any GPIO write operation. If you want to experiment with other values (in microseconds), use -t.