#pragma once
#include <string>


#ifndef DIAL_STRUCTS
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

struct DIALAppCallbacks {
    DIAL_app_start_cb start_cb;
    DIAL_app_hide_cb hide_cb;
    DIAL_app_stop_cb stop_cb;
    DIAL_app_status_cb status_cb;
};
#endif

extern "C"{
typedef bool (*pParseConfig)( std::string key, std::string value ); 
typedef void (*pValidateValues)();
}
struct  serviceData{	
	pParseConfig parseConfig;
	pValidateValues validateValues;
	const char  *name;
	void *user_data;
	DIALAppCallbacks callbacks;
	int useAdditionalData;
	const char *corsAllowedOrigin;
};


extern "C"{
typedef serviceData (*pGetServiceData)();
serviceData GetServiceData();
}