 // Copyright (c) 2004-2010 Sergey Lyubka
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


// NOTE: This is a SEVERELY stripped down version of mongoose, which only
// supports GET, POST and DELETE HTTP commands, no CGI, no file or directory
// access, no ACLs or authentication, and no proxying and no SSL.  HTTP Header
// limit is 16 instead of 64, as it's not supposed to be called from standard
// browsers. And most options are removed.

#pragma once

#include <Ws2tcpip.h>

#include <windows.h>
#include <winsock.h>
#include <cstdint>
#include <mutex>
typedef SOCKADDR_IN sockaddr_in;


#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

enum mg_event {
  MG_NEW_REQUEST,   // New HTTP request has arrived from the client
  MG_HTTP_ERROR,    // HTTP error must be returned to the client
  MG_EVENT_LOG,     // Mongoose logs an event, request_info.log_message
};

struct _socket{
  int sock;          // Listening socket
  struct sockaddr_in local_addr;  // Local socket address
  struct sockaddr_in remote_addr;  // Remote socket address
};


// This structure contains information about the HTTP request.
struct mg_request_info {
  void *user_data;       // User-defined pointer passed to mg_start()
  char *request_method;  // "GET", "POST", etc
  char *uri;             // URL-decoded URI
  char *http_version;    // E.g. "1.0", "1.1"
  char *query_string;    // \0 - terminated
  char *request_body;    // \0 - terminated
  char *log_message;     // Mongoose error log message
  sockaddr_in local_addr; // Our server's address for this connection
  sockaddr_in remote_addr; // The remote address for this connection
  int status_code;       // HTTP reply status code
  int num_headers;       // Number of headers
  struct mg_header {
    char *name;          // HTTP header name
    char *value;         // HTTP header value
  };
  mg_header http_headers[16];    // Maximum 16 headers
};

struct mg_context;
class mg_connection;
typedef void * (*mg_callback_t)(mg_event, mg_connection*,const mg_request_info*);
typedef void * (*mg_thread_func_t)(void *);

class mg_connection {
  mg_request_info request_info;
  mg_context *ctx;
  _socket client;       // Connected client
  time_t birth_time;          // Time connection was accepted
  int64_t num_bytes_sent;     // Total bytes sent to client
  int64_t content_len;        // Content-Length header value
  int64_t consumed_content;   // How many bytes of content is already read
  char *buf;                  // Buffer for received data
  int buf_size;               // Buffer size
  int request_len;            // Size of the request + headers in a buffer
  int data_len;               // Total size of data in a buffer
	
  void reset_per_request_attributes();
  
  void close_connection();
  void *call_user(enum mg_event event);
  int vsnprintf(char *buf, size_t buflen, const char *fmt, va_list ap);
  int snprintf(char *buf, size_t buflen, const char *fmt, ...);
  const char* suggest_connection_header();
  void handle_request();
  void process_new_connection();
  void discard_current_request_from_buffer();
	
  public:
  
  void cry(const char *fmt, ...) ;
  mg_connection(mg_context* ctx);
  friend int start_thread(struct mg_context *ctx, mg_thread_func_t func, void *param);
  friend void worker_thread(mg_context*);
  void stop( mg_context *);
  int write( const void *buf, size_t len);
  int printf( const char *fmt, ...);
  int read( void *buf, size_t len);
  const char* get_header(const char* name);
  void send_http_error(int status, const char *reason, const char *fmt, ...);
  
  
  
};

mg_context* mg_start(mg_callback_t callback, void *user_data, int port);

int mg_get_listen_addr( mg_context* ctx, sockaddr *addr, socklen_t *addrlen);

struct mg_context {
  volatile int stop_flag;       // Should we stop event loop
  mg_callback_t user_callback;  // User-defined callback function
  void *user_data;              // User-defined data

  int             local_socket;
  sockaddr_in local_address;

  volatile int num_threads;  // Number of threads
  pthread_mutex_t mutex;     // Protects (max|num)_threads
  pthread_cond_t  cond;      // Condvar for tracking workers terminations
  _socket queue[20];   // Accepted sockets
  volatile int sq_head;      // Head of the socket queue
  volatile int sq_tail;      // Tail of the socket queue
  pthread_cond_t sq_full;    // Singaled when socket is produced
  pthread_cond_t sq_empty;   // Signaled when socket is consumed
}; 

void mg_stop(mg_context *ctx) ;



// Return Mongoose version.
const char *mg_version(void);



void mg_md5(char *buf, ...);



#ifdef __cplusplus
}
#endif // __cplusplus


