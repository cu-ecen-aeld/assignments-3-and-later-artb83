# TCP server and Linux char-device driver review details
**Built as part of the University of Colorado Advanced Embedded Linux course.**
## Description
A multithreaded TCP server daemon plus a Linux char-device driver based on circular buffer with ioctl seek.
## Location
TCP server at **_./server_**, char-device driver at **_./aesd-char-driver_**
## How to build
To build the server (aesdsocket) application natively:  
`cd server`  
`make`  
Cross-compiling with CROSS_COMPILE=aarch64-none-linux-gnu-   
`make CROSS_COMPILE=aarch64-none-linux-gnu-`  
*Or through the Yocto layer.*  
To build the char-device:  
`cd aesd-char-driver`  
`make modules`
## How to test
`cd ./server` folder, run:  
`sudo ./reload_aesdchar_driver_run_tcpserver -a
`
### Yocto integration
Both components are also packaged for embedded Linux: they are built into a  
Yocto (Kirkstone) image through recipes in a custom layer, which fetch this  
repo, cross-compile `aesdsocket` and the `aesdchar` kernel module, and install  
them with init scripts so the driver loads and the server starts at boot.  
See [Yocto repo](https://github.com/cu-ecen-aeld/assignment-9-artb83)
