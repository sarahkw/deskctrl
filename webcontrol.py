#!/usr/bin/env python3

import http.server

class WebControl(http.server.BaseHTTPRequestHandler):
    pass

if __name__ == "__main__":
    httpd = http.server.HTTPServer(('', 8080),
                                   WebControl)
    httpd.serve_forever()
