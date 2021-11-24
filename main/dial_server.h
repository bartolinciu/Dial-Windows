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

#pragma once
#include <cstdint>
#include <Ws2tcpip.h>
#include <winsock2.h>
#include "mongoose.h"
#define DIAL_STRUCTS
typedef uint16_t in_port_t;
typedef void * DIAL_run_t;

enum DIALStatus{
    kDIALStatusStopped,
    kDIALStatusHide,
    kDIALStatusRunning
};

typedef DIALStatus (*DIAL_app_start_cb)(const char*, const char*, const char*, DIAL_run_t*, void*);
typedef DIALStatus (*DIAL_app_hide_cb)(const char *, DIAL_run_t *, void *);
typedef void (*DIAL_app_stop_cb)(const char *, DIAL_run_t, void *);
typedef DIALStatus (*DIAL_app_status_cb)(const char *, DIAL_run_t, int*, void *);


//#define DEBUG
#ifdef DEBUG
#define ATRACE(...) printf(__VA_ARGS__)
#else
#define ATRACE(...)
#endif



#define DIAL_VERSION ("\"2.1\"")
#define DIAL_MAX_PAYLOAD (4096)
#define DIAL_MAX_ADDITIONALURL (1024)

struct DIALAppCallbacks {
    DIAL_app_start_cb start_cb;
    DIAL_app_hide_cb hide_cb;
    DIAL_app_stop_cb stop_cb;
    DIAL_app_status_cb status_cb;
};

struct DIALApp {
    struct DIALApp *next;
    struct DIALAppCallbacks callbacks;
    struct DIALData_ *dial_data;
    void *callback_data;
    DIAL_run_t run_id;
    DIALStatus state;
    char *name;
    char payload[DIAL_MAX_PAYLOAD];
    int useAdditionalData;
    char corsAllowedOrigin[256];
};

class DIALServer {
	mg_context *ctx;
    DIALApp *apps;
    std::mutex mux;
	
	public:
	
	DIALServer();
	void lock();
	void unlock();
	DIALApp **find_app(const char *app_name);
	int is_allowed_origin(char * origin, const char * app_name);
	void *options_response(struct mg_connection *conn, char *host_header, char *origin_header, const char* app_name, const char* methods);
	void start();
	void stop();
	in_port_t get_port();
	int register_app(const char *app_name, struct DIALAppCallbacks *callbacks, void *user_data, int useAdditionalData, const char* corsAllowedOrigin);
	int unregister_app(const char *app_name);
	const char * get_payload(const char *app_name);
};

int should_check_for_origin( char * origin );
int ends_with_in_list (const char *str, const char *list);
void *request_handler(enum mg_event event, mg_connection *conn, const mg_request_info *request_info);



