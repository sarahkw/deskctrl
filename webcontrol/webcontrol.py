#!/usr/bin/env python3

import http.server
import json

class SarahsDesk(object):
    def put(self, data):
        return {"reply" : data}

OBJECTS = {
    "/sarahsdesk" : SarahsDesk(),
}

class WebControl(http.server.SimpleHTTPRequestHandler):
    def do_PUT(self):
        if self.path in OBJECTS:
            size = int(self.headers.get("content-length"))
            output = OBJECTS[self.path].put(json.loads(self.rfile.read(size).decode("UTF-8")))
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            response = json.dumps(output).encode("UTF-8")
            self.wfile.write(response)
        else:
            self.send_response(404)

if __name__ == "__main__":
    httpd = http.server.HTTPServer(('', 8080),
                                   WebControl)
    httpd.serve_forever()
