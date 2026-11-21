#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// Initialize HTTP server routes, CORS, and LittleFS static file serving
void web_server_init();

// Process incoming HTTP requests in non-blocking loop turn
void web_server_handle_client();

#endif // WEB_SERVER_H

