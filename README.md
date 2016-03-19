# Synopsis

Reliable UDP is a course-related project assigned in an upper-division networks [class](http://www.registrar.ucla.edu/schedule/subdet.aspx?srs=187350200&term=16W&session=) at UCLA. A networking layer is built on top of UDP to create a client-server interaction that exhibits reliable data transfer.

# Development

Selective repeat is implemented to ensure reliable transfer of data between endpoints. Because the actual rate of packet loss and corruption in a local area network (LAN) may be too low, we created methods to simulate packet loss and corruption for testing purposes. 

# Installation

To compile and build this project, simply type the following in a Linux terminal:

    make

The Makefile compiles the code using g++.
