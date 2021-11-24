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
#define _WIN32_WINNT _WIN32_WINNT_WIN8 // Windows 8.0
#include <Ws2tcpip.h>
#include <winsock2.h>
#include "dial_data.h"
#include "dial_server.h"

#include <pthread.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>


#include "url_lib.h"
#include "LinuxInterfaces.h"

// TODO: Partners should define this port
#define DIAL_PORT  (56789)
#define DIAL_DATA_SIZE (8*1024)

static const char *gLocalhost = "127.0.0.1";

DIALServer::DIALServer(){
	this->apps = NULL;
}

void DIALServer::lock(){
	this->mux.lock();
}

void DIALServer::unlock(){
	this->mux.unlock();
}

DIALApp** DIALServer::find_app(const char *app_name) {
	DIALApp *app;
	DIALApp **ret = &this->apps;
		for (app = this->apps; app != NULL; ret = &app->next, app = app->next) {
		if (!strcmp(app_name, app->name)) {
			break;
		}
	}
	return ret;
}
	
int DIALServer::is_allowed_origin(char * origin, const char * app_name) {
	if (!origin || strlen(origin)==0 || !should_check_for_origin(origin)) {
		return 1;
	}
	this->lock();
	DIALApp *app;
	int result = 0;
	for (app = this->apps; app != NULL; app = app->next) {
		if (!strcmp(app->name, app_name)) {
			if (!app->corsAllowedOrigin[0] ||
				ends_with_in_list(origin, app->corsAllowedOrigin)) {
				result = 1;
				break;
			}
		}
	}
	this->unlock();
	return result;
}

void* DIALServer::options_response(mg_connection *conn, char *host_header, char *origin_header, const char* app_name, const char* methods){    
	if (host_header && this->is_allowed_origin(origin_header, app_name)) {
		conn->printf(
				  "HTTP/1.1 204 No Content\r\n"
				  "Access-Control-Allow-Methods: %s\r\n"
				  "Access-Control-Max-Age: 86400\r\n"
				  "Access-Control-Allow-Origin: %s\r\n"
				  "Content-Length: 0"
				  "\r\n",
				  methods,
				  origin_header);
		return(void*) "done";
	}
	conn->send_http_error(403, "Forbidden", "Forbidden");
	return(void*) "done";   
}

void DIALServer::start() {
	this->ctx = mg_start(request_handler, this, DIAL_PORT);
}

void DIALServer::stop() {
	mg_stop(this->ctx);
}

in_port_t DIALServer::get_port() {
	struct sockaddr sa;
	socklen_t len = sizeof(sa);
	if (!mg_get_listen_addr(this->ctx, &sa, &len)) 
		return 0;
	
	return ntohs(((struct sockaddr_in *) &sa)->sin_port);
}

int DIALServer::register_app(const char *app_name,
                     struct DIALAppCallbacks *callbacks, void *user_data,
                     int useAdditionalData,
                     const char* corsAllowedOrigin) {
	DIALApp **ptr, *app;
	int ret;
	this->lock();
	ptr = this->find_app(app_name);
	if (*ptr != NULL) {  // app already registered
		ret = 0;
	} else {
		app = (DIALApp*) malloc(sizeof(DIALApp));
		app->callbacks = *callbacks;
		app->name = strdup(app_name);
		app->next = *ptr;
		app->state = kDIALStatusStopped;
		app->callback_data = user_data;
		app->dial_data = retrieve_dial_data(app->name);
		app->useAdditionalData = useAdditionalData;
		app->corsAllowedOrigin[0] = '\0';
		if (corsAllowedOrigin &&
			strlen(corsAllowedOrigin) < sizeof(app->corsAllowedOrigin)) {
		  strcpy(app->corsAllowedOrigin, corsAllowedOrigin);
		}
		
		*ptr = app;
		ret = 1;
	}
	this->unlock();
	return ret;
}

int DIALServer::unregister_app(const char *app_name) {
	DIALApp **ptr, *app;
	int ret;
	this->lock();
	ptr = this->find_app(app_name);
	if (*ptr == NULL) {  // no such app
		ret = 0;
	} else {
		app = *ptr;
		*ptr = app->next;
		free(app->name);
		free(app);
		ret = 1;
	}
	this->unlock();
	return ret;
}

const char* DIALServer::get_payload(const char *app_name) {
	const char * pPayload = NULL;
	DIALApp **ptr, *app;
	// NOTE: Don't grab the mutex as we are calling this function from
	// inside the application callback which already has the lock.
	//ds_lock(ds);
	ptr = this->find_app(app_name);
	if (*ptr != NULL) {
		app = *ptr;
		pPayload = app->payload;
	}
	//ds_unlock(ds);
	return pPayload;
}

static void url_decode_xml_encode(char *dst, char *src, size_t src_size) {
    char *url_decoded_key = (char *) malloc(src_size + 1);
    urldecode(url_decoded_key, src, src_size);
    xmlencode(dst, url_decoded_key, 2 * src_size);
    free(url_decoded_key);
}

/*
 * A bad payload is defined to be an unprintable character or a
 * non-ascii character.
 */
static int isBadPayload(const char* pPayload, int numBytes) {
    int i = 0;
    fprintf( stderr, "Payload: checking %d bytes\n", numBytes);
    for (; i < numBytes; i++) {
        // High order bit should not be set
        // 0x7F is DEL (non-printable)
        // Anything under 32 is non-printable
        if (((pPayload[i] & 0x80) == 0x80) || (pPayload[i] == 0x7F)
                || (pPayload[i] <= 0x1F))
            return 1;
    }
    return 0;
}

static void handle_app_start(struct mg_connection *conn,
                             const struct mg_request_info *request_info,
                             const char *app_name,
                             const char *origin_header) {
    char additional_data_param[256] = {0, };
    char body[DIAL_MAX_PAYLOAD + sizeof(additional_data_param) + 2] = {0, };
    DIALApp *app;
    DIALServer *ds = (DIALServer*) request_info->user_data;
    int body_size;

    ds->lock();
    app = *(ds->find_app( app_name));
    if (!app) {
        conn->send_http_error(404, "Not Found", "Not Found");
    } else {
        body_size = conn->read(body, sizeof(body));
        // NUL-terminate it just in case
        if (body_size > DIAL_MAX_PAYLOAD) {
            conn->send_http_error(413, "413 Request Entity Too Large",
                               "413 Request Entity Too Large");
        } else if (isBadPayload(body, body_size)) {
            conn->send_http_error(400, "400 Bad Request", "400 Bad Request");
        } else {
            char laddr[INET6_ADDRSTRLEN];
            const struct sockaddr_in *addr =
                    (struct sockaddr_in *) &request_info->local_addr;
            inet_ntop(addr->sin_family, (void*) &addr->sin_addr, laddr, sizeof(laddr));
            in_port_t dial_port = ds->get_port();

            if (app->useAdditionalData) {
                // Construct additionalDataUrl=http://host:port/apps/app_name/dial_data
                sprintf(additional_data_param,
                        "additionalDataUrl=http%%3A%%2F%%2Flocalhost%%3A%d%%2Fapps%%2F%s%%2Fdial_data%%3F",
                        dial_port, app_name);
            }
            fprintf(stderr, "Starting the app with params %s\n", body);
            app->state = app->callbacks.start_cb( app_name, body,
                                                 additional_data_param, 
                                                 &app->run_id,
                                                 app->callback_data);
            if (app->state == kDIALStatusRunning) {
                conn->printf(
                        "HTTP/1.1 201 Created\r\n"
                        "Content-Type: text/plain\r\n"
                        "Location: http://%s:%d/apps/%s/run\r\n"
                        "Access-Control-Allow-Origin: %s\r\n"
                        "\r\n",
                        laddr, dial_port, app_name, origin_header);
                // copy the payload into the application struct
                memset(app->payload, 0, DIAL_MAX_PAYLOAD);
                memcpy(app->payload, body, body_size);
            } else {
                conn->send_http_error(503, "Service Unavailable",
                                   "Service Unavailable");
            }
        }
    }
    ds->unlock();
}

static void handle_app_status(struct mg_connection *conn,
                              const struct mg_request_info *request_info,
                              const char *app_name,
                              const char *origin_header) {
    DIALApp *app;
    int canStop = 0;
    DIALServer *ds = (DIALServer*) request_info->user_data;

    // determin client version
    char *clientVersionStr = parse_param(request_info->query_string, (char*) "clientDialVer");
    double clientVersion = 0.0;
    if (clientVersionStr){
        clientVersion = atof(clientVersionStr);
        free(clientVersionStr);
    }
    
    ds->lock();
    app = *(ds->find_app(app_name));
    if (!app) {
        conn->send_http_error(404, "Not Found", "Not Found");
        ds->unlock();
        return;
    }

    char dial_data[DIAL_DATA_SIZE] = {0,};
    char *end = dial_data + DIAL_DATA_SIZE;
    char *p = dial_data;

    for (DIALData* first = app->dial_data; first != NULL; first = first->next) {
        p = smartstrcat(p,(char*) "    <", end - p);
        size_t key_length = strlen(first->key);
        char *encoded_key = (char *) malloc(2 * key_length + 1);
        url_decode_xml_encode(encoded_key, first->key, key_length);

        size_t value_length = strlen(first->value);
        char *encoded_value = (char *) malloc(2 * value_length + 1);
        url_decode_xml_encode(encoded_value, first->value, value_length);

        p = smartstrcat(p, encoded_key, end - p);
        p = smartstrcat(p,(char*) ">", end - p);
        p = smartstrcat(p, encoded_value, end - p);
        p = smartstrcat(p,(char*) "</", end - p);
        p = smartstrcat(p, encoded_key, end - p);
        p = smartstrcat(p,(char*) ">", end - p);
        free(encoded_key);
        free(encoded_value);
    }

    app->state = app->callbacks.status_cb(app_name, app->run_id, &canStop, app->callback_data);

    DIALStatus localState = app->state;
    
    // overwrite app->state if cilent version < 2.1    
    if (clientVersion < 2.09 && localState==kDIALStatusHide){
        localState=kDIALStatusStopped;
    }
    
    char dial_state_str[20];
    switch(localState){
    case kDIALStatusHide:
        strcpy (dial_state_str, "hidden");
        break;
    case kDIALStatusRunning:
        strcpy (dial_state_str, "running");
        break;
    default:
        strcpy (dial_state_str, "stopped");
    }
    
    conn->printf(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/xml\r\n"
            "Access-Control-Allow-Origin: %s\r\n"
            "\r\n"
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
            "<service xmlns=\"urn:dial-multiscreen-org:schemas:dial\" dialVer=%s>\r\n"
            "  <name>%s</name>\r\n"
            "  <options allowStop=\"%s\"/>\r\n"
            "  <state>%s</state>\r\n"
            "%s"
            "  <additionalData>\n"
            "%s"
            "\n  </additionalData>\n"
            "</service>\r\n",
            origin_header,            
            DIAL_VERSION,
            app->name,
            canStop ? "true" : "false",
            dial_state_str,
            localState == kDIALStatusStopped ?
                    "" : "  <link rel=\"run\" href=\"run\"/>\r\n",
            dial_data);
    ds->unlock();
}

static void handle_app_stop(mg_connection *conn,
                            const mg_request_info *request_info,
                            const char *app_name,
                            const char *origin_header) {
    DIALApp *app;
    DIALServer *ds = (DIALServer*) request_info->user_data;
    int canStop = 0;

    ds->lock();
    app = *(ds->find_app(app_name));

    // update the application state
    if (app) {
        app->state = app->callbacks.status_cb(app_name, app->run_id,
                                              &canStop, app->callback_data);
    }

    if (!app || app->state == kDIALStatusStopped) {
        conn->send_http_error(404, "Not Found", "Not Found");
    } else {
        app->callbacks.stop_cb(app_name, app->run_id, app->callback_data);
        app->state = kDIALStatusStopped;
        conn->printf("HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/plain\r\n"
                  "Access-Control-Allow-Origin: %s\r\n"
                  "\r\n",
                  origin_header);
    }
    ds->unlock();
}

static void handle_app_hide(mg_connection *conn,
                            const mg_request_info *request_info,
                            const char *app_name,
                            const char *origin_header) {
    DIALApp *app;
    DIALServer *ds =(DIALServer*) request_info->user_data;
    int canStop = 0;

    ds->lock();
    app = *(ds->find_app(app_name));
  
    // update the application state
    if (app) {
        app->state = app->callbacks.status_cb(app_name, app->run_id,
                                              &canStop, app->callback_data);
    }
    
    if (!app || (app->state != kDIALStatusRunning && app->state != kDIALStatusHide)) {
        conn->send_http_error(404, "Not Found", "Not Found");
    } else {
        // not implemented in reference
        DIALStatus status = app->callbacks.hide_cb(app_name, &app->run_id, app->callback_data);
        if (status!=kDIALStatusHide){
            fprintf(stderr, "Hide not implemented for reference.\n");
            conn->send_http_error(501, "Not Implemented",
                               "Not Implemented");
        }else{
        app->state = kDIALStatusHide;
        conn->printf("HTTP/1.1 200 OK\r\n"
                  "Content-Type: text/plain\r\n"
                  "Access-Control-Allow-Origin: %s\r\n"
                  "\r\n",
                  origin_header);
        }
    }
    ds->unlock();
}

static void handle_dial_data(mg_connection *conn,
                             const mg_request_info *request_info,
                             const char *app_name,
                             const char *origin_header,
                             int use_payload) {
    char body[DIAL_DATA_MAX_PAYLOAD + 2] = {0, };

    DIALApp *app;
    DIALServer *ds =(DIALServer*) request_info->user_data;

    ds->lock();
    app = *(ds->find_app(app_name));
    if (!app) {
        conn->send_http_error(404, "Not Found", "Not Found");
        ds->unlock();
        return;
    }
    int nread;
    if (!use_payload) {
        if (request_info->query_string) {
            strncpy(body, request_info->query_string, DIAL_DATA_MAX_PAYLOAD);
            nread = strlen(body);
        } else {
          nread = 0;
        }
    } else {
        nread = conn->read(body, DIAL_DATA_MAX_PAYLOAD);
        body[nread] = '\0';
    }
    if (nread > DIAL_DATA_MAX_PAYLOAD) {
        conn->send_http_error(413, "413 Request Entity Too Large",
                           "413 Request Entity Too Large");
        ds->unlock();
        return;
    }

    if (isBadPayload(body, nread)) {
        conn->send_http_error(400, "400 Bad Request", "400 Bad Request");
        ds->unlock();
        return;
    }


    free_dial_data(&app->dial_data);

    app->dial_data = parse_params(body);
    store_dial_data(app->name, app->dial_data);

    conn->printf("HTTP/1.1 200 OK\r\n"
              "Access-Control-Allow-Origin: %s\r\n"
              "\r\n",
              origin_header);

    ds->unlock();
}

static int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


// str contains a white space separated list of strings (only supports SPACE  characters for now)
int ends_with_in_list (const char *str, const char *list) {
    if (!str || !list)
        return 0;
    
    const char * scanPointer=list;
    const char * spacePointer;
    unsigned int substringSize = 257;
    char *substring = (char *)malloc(substringSize);
    if (!substring){
        return 0;
    }
    while ( (spacePointer =strchr(scanPointer, ' ')) != NULL) {
    	int copyLength = spacePointer - scanPointer;      
      
      // protect against buffer overflow
      if (copyLength>=substringSize){
          substringSize=copyLength+1;
          free(substring);
          substring=(char *)malloc(substringSize);
          if (!substring){
              return 0;
          }
      }

    	memcpy(substring, scanPointer, copyLength);
    	substring[copyLength] = '\0';
    	//printf("found %s \n", substring);
    	if (ends_with(str, substring)) {
          free(substring);
          return 1;
    	}
    	scanPointer = scanPointer + copyLength + 1; // assumption: only 1 character
    }
    free(substring);
    return ends_with(str, scanPointer);
}

int should_check_for_origin( char * origin ) {
    const char * const CHECK_PROTOS[] = { "http:", "https:", "file:" };
    for (int i = 0; i < 3; ++i) {
        if (!strncmp(origin, CHECK_PROTOS[i], strlen(CHECK_PROTOS[i]) - 1)) {
            return 1;
        }
    }
    return 0;
}



#define APPS_URI "/apps/"
#define RUN_URI "/run"
#define HIDE_URI "/hide"



void *request_handler(mg_event event, mg_connection *conn, const mg_request_info *request_info) {
    DIALServer *ds = (DIALServer*) request_info->user_data;

    fprintf(stderr, "Received request %s\n", request_info->uri);
    char *host_header = {0,};
    char *origin_header = {0,};
    for (int i = 0; i < request_info->num_headers; ++i) {
        if (!strcmp(request_info->http_headers[i].name, "Host")  ||
            !strcmp(request_info->http_headers[i].name, "host")) {
            host_header = request_info->http_headers[i].value;
        } else if (!strcmp(request_info->http_headers[i].name, "Origin") ||
                   !strcmp(request_info->http_headers[i].name, "origin")) {
            origin_header = request_info->http_headers[i].value;
        }
    }
    fprintf(stderr, "Origin %s, Host: %s\n", origin_header, host_header);
    if (event == MG_NEW_REQUEST) {
        // URL ends with run
        if (!strncmp(request_info->uri + strlen(request_info->uri) - 4, RUN_URI,
                     strlen(RUN_URI))) {
            char app_name[256] = {0, };  // assuming the application name is not over 256 chars.
            strncpy(app_name, request_info->uri + strlen(APPS_URI),
                    ((strlen(request_info->uri) - 4) - (sizeof(APPS_URI) - 1)));

            if (!strcmp(request_info->request_method, "OPTIONS")) {
                return ds->options_response(conn, host_header, origin_header, app_name, "DELETE, OPTIONS");
            }

            // DELETE non-empty app name
            if (app_name[0] != '\0'
                    && !strcmp(request_info->request_method, "DELETE")) {
                if (host_header && ds->is_allowed_origin(origin_header, app_name)) {
                    handle_app_stop(conn, request_info, app_name, origin_header);
                } else {
                    conn->send_http_error(403, "Forbidden", "Forbidden");
                    return (void*) "done";
                }
            } else {
                conn->send_http_error(501, "Not Implemented",
                                   "Not Implemented");
            }
        }
        // URI starts with "/apps/" and is followed by an app name
        else if (!strncmp(request_info->uri, APPS_URI, sizeof(APPS_URI) - 1)
                && !strchr(request_info->uri + strlen(APPS_URI), '/')) {
            const char *app_name;
            app_name = request_info->uri + sizeof(APPS_URI) - 1;

            if (!strcmp(request_info->request_method, "OPTIONS")) {
                return ds->options_response(conn, host_header, origin_header, app_name, "GET, POST, OPTIONS");
            }

            // start app
            if (!strcmp(request_info->request_method, "POST")) {
                if (host_header && ds->is_allowed_origin(origin_header, app_name)) {
                    handle_app_start(conn, request_info, app_name, origin_header);
                } else {
                    conn->send_http_error(403, "Forbidden", "Forbidden");
                    return (void*) "done";
                }
            // get app status
            } else if (!strcmp(request_info->request_method, "GET")) {
                handle_app_status(conn, request_info, app_name, origin_header);
            } else {
                conn->send_http_error(501, "Not Implemented",
                                  "Not Implemented");
            }
        }
        // URI that ends with HIDE_URI
        else if (!strncmp(request_info->uri + strlen(request_info->uri) - strlen(HIDE_URI), HIDE_URI,
                     strlen(HIDE_URI))) {
            char app_name[256] = {0, };  // assuming the application name is not over 256 chars.
            strncpy(app_name, request_info->uri + strlen(APPS_URI),
                    ((strlen(request_info->uri) - strlen(RUN_URI) - strlen(HIDE_URI)) - (sizeof(APPS_URI) - 1)));

            if (!strcmp(request_info->request_method, "OPTIONS")) {
                return ds->options_response(conn, host_header, origin_header, app_name, "POST, OPTIONS");
            }
            
            if (app_name[0] != '\0'                
                && !strcmp(request_info->request_method, "POST")) {
                handle_app_hide(conn, request_info, app_name, origin_header);
            }else{
                conn->send_http_error(501, "Not Implemented",
                                   "Not Implemented");
            }
        }
        // URI is of the form */app_name/dial_data
        else if (strstr(request_info->uri, DIAL_DATA_URI)) {
            char laddr[INET6_ADDRSTRLEN];
            const struct sockaddr_in *addr =
                    (struct sockaddr_in *) &request_info->remote_addr;
            InetNtop(addr->sin_family,(void*) &addr->sin_addr, laddr, sizeof(laddr));
            if ( !strncmp(laddr, gLocalhost, strlen(gLocalhost)) ) {
                char *app_name = parse_app_name(request_info->uri);

                if (!strcmp(request_info->request_method, "OPTIONS")) {
                    void *ret = ds->options_response(conn, host_header, origin_header, app_name, "POST, OPTIONS");
                    free(app_name);
                    return ret;
                }
                int use_payload =
                    strcmp(request_info->request_method, "POST") ? 0 : 1;
                handle_dial_data(conn, request_info, app_name, origin_header,
                                 use_payload);

                free(app_name);
            } else {
                // If the request is not from local host, return an error
                conn->send_http_error(403, "Forbidden", "Forbidden");
            }
        } else {
           conn->send_http_error( 404, "Not Found", "Not Found");
        }
        return (void*) "done";
    } else if (event == MG_EVENT_LOG) {
        fprintf( stderr, "MG: %s\n", request_info->log_message);
        return (void*) "done";
    }
    return NULL;
}




