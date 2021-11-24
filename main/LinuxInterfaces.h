#ifndef LINUXINTERFACES_H_
#define LINUXINTERFACES_H_


#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/* Fetch the MACAddress of a network interface */
#define MACBUFSIZE 32
#define WAKEUPTIMEOUT 15

typedef struct
{
    char macAddress[MACBUFSIZE];
}NetInterface;

NetInterface getDefaultNetworkInterfaces();

#endif 
