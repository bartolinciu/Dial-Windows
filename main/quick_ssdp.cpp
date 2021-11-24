/*
* Copyright (c) 2014 Netflix, Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY NETFLIX, INC. AND CONTRIBUTORS "AS IS" AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL NETFLIX OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mongoose.h"
#include <stdbool.h>


// TODO: Partners should define this port
#define SSDP_PORT (56790)
static char gBuf[4096];

// TODO: Partners should get the friendlyName from the system and insert here.
// TODO: Partners should get the UUID from the system and insert here.
static const char ddxml[] = ""
    "<?xml version=\"1.0\"?>"
    "<root"
    " xmlns=\"urn:schemas-upnp-org:device-1-0\""
    " xmlns:r=\"urn:restful-tv-org:schemas:upnp-dd\">"
    " <specVersion>"
    " <major>1</major>"
    " <minor>0</minor>"
    " </specVersion>"
    " <device>"
    " <deviceType>urn:schemas-upnp-org:device:tvdevice:1</deviceType>"
    " <friendlyName>%s</friendlyName>"
    " <manufacturer> </manufacturer>"
    " <modelName>%s</modelName>"
    " <UDN>uuid:%s</UDN>"
    " </device>"
    "</root>";


// TODO: Partners should get the UUID from the system and insert here.
static const char ssdp_reply[] = "HTTP/1.1 200 OK\r\n"
    "LOCATION: http://%s:%d/dd.xml\r\n"
    "CACHE-CONTROL: max-age=1800\r\n"
    "EXT:\r\n"
    "BOOTID.UPNP.ORG: 1\r\n"
    "SERVER: Linux/2.6 UPnP/1.1 quick_ssdp/1.1\r\n"
    "ST: urn:dial-multiscreen-org:service:dial:1\r\n"
    "USN: uuid:%s::"
    "urn:dial-multiscreen-org:service:dial:1\r\n"
    "\r\n";

extern char ip_addr[INET_ADDRSTRLEN];
static int dial_port = 0;
static int my_port = 0;
static char friendly_name[256];
static char uuid[256];
static char model_name[256];
static struct mg_context *ctx;

bool wakeOnWifiLan=false;

static void *request_handler(mg_event event,
                             mg_connection *conn,
                             const mg_request_info *request_info) {
    if (event == MG_NEW_REQUEST) {
        if (!strcmp(request_info->uri, "/dd.xml") &&
            !strcmp(request_info->request_method, "GET")) {
            conn->printf( "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/xml\r\n"
                      "Application-URL: http://%s:%d/apps/\r\n"
                      "\r\n", ip_addr, dial_port);
            conn->printf(ddxml, friendly_name, model_name, uuid);
        } else {
            conn->send_http_error(404, "Not Found", "Not Found");
        }
        return (char*) "done";
    }
    return NULL;
}


static void handle_mcast() {
    int s, one = 1, bytes;
    socklen_t addrlen;
    sockaddr_in saddr;
    ip_mreq mreq;
    char send_buf[sizeof(ssdp_reply) + INET_ADDRSTRLEN + 256 + 256] = {0,};
    int send_size;
    send_size = snprintf(send_buf, sizeof(send_buf), ssdp_reply, ip_addr, my_port, uuid);
    if (-1 == (s = socket(AF_INET, SOCK_DGRAM, 0))) {
        perror("socket");
        exit(1);
    }
    if (-1 == setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &one, sizeof(one))) {
        perror("reuseaddr");
        exit(1);
    }
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;// inet_addr(ip_addr);// inet_addr("239.255.255.250");
    saddr.sin_port = htons(1900);

    if (SOCKET_ERROR == bind(s, (sockaddr *)&saddr, sizeof(saddr))) {
        printf("error: %i\n", WSAGetLastError());
		printf("%u.%u.%u.%u\n", saddr.sin_addr.S_un.S_un_b.s_b1, saddr.sin_addr.S_un.S_un_b.s_b2, saddr.sin_addr.S_un.S_un_b.s_b3, saddr.sin_addr.S_un.S_un_b.s_b4);
		printf("%u\n", sizeof(saddr));
		perror("bind");
        exit(1);
    }
    mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
    mreq.imr_interface.s_addr =  inet_addr(ip_addr);
    if (-1 == setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                         (char*)&mreq, sizeof(mreq))) {
        perror("add_membership");
        exit(1);
    }
    //printf("Starting Multicast handling on 239.255.255.250\n");
    while (1) {
        addrlen = sizeof(saddr);
        if (-1 == (bytes = recvfrom(s, gBuf, sizeof(gBuf) - 1, 0,
                                    (sockaddr *)&saddr, &addrlen))) {
            perror("recvfrom");
            continue;
        }
        gBuf[bytes] = 0;
        // sophisticated SSDP parsing algorithm
        if (!strstr(gBuf, "urn:dial-multiscreen-org:service:dial:1"))
            {
#if 0 // use for debugging
                printf("Dropping: \n");
                {
                    int i;
                    for (i = 0; i < bytes; i++)
                        {
                            putchar(gBuf[i]);
                        }
                }
                printf("\n##### End of DROP #######\n");
#endif
                continue;
        }
        printf("Sending SSDP reply to %s:%d\n",
               inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port));
        if (-1 == sendto(s, send_buf, send_size, 0, (sockaddr *)&saddr, addrlen)) {
            perror("sendto");
            continue;
        }
    }
}

void run_ssdp(int port, const char *pFriendlyName, const char * pModelName, const char *pUuid) {
    sockaddr sa;
    socklen_t len = sizeof(sa);
    if(pFriendlyName) {
        strncpy(friendly_name, pFriendlyName, sizeof(friendly_name));
        friendly_name[255] = '\0';
    } else {
        strcpy(friendly_name, "DIAL server sample");
    }
    if(pModelName) {
        strncpy(model_name, pModelName, sizeof(model_name));
        uuid[255] = '\0';
    } else {
        strcpy(model_name, "deadbeef-dead-beef-dead-beefdeadbeef");
    }
    if(pUuid) {
        strncpy(uuid, pUuid, sizeof(uuid));
        uuid[255] = '\0';
    } else {
        strcpy(uuid, "deadbeef-dead-beef-dead-beefdeadbeef");
    }
    dial_port = port;
    ctx = mg_start(&request_handler, NULL, SSDP_PORT);
    if (mg_get_listen_addr(ctx, &sa, &len)) {
        my_port = ntohs(((sockaddr_in *)&sa)->sin_port);
    }
    printf("SSDP listening on %s:%d\n", ip_addr, my_port);
    handle_mcast();
}
