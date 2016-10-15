#!/usr/bin/env python3

import http.server
import json


class SarahsDesk(object):
    class HelpfulError(Exception):
        pass

    def put_height_const(self, height):
        # TODO
        print("TODO: Set height to {}".format(height))

    def put_height_move(self, duration, direction):
        assert direction in ("up", "down")
        # TODO
        print("TODO: Move for {} ms towards {}".format(duration, direction))

    def put_height_preset(self, preset):
        # TODO
        print("TODO: Set height to preset {}".format(preset))

    def get(self):
        # TODO
        return {"height": None}

    def _put_internal(self, data):
        reply = {}
        ((command, data), ) = data.items()
        if command == "height":
            ((subcommand, data), ) = data.items()
            if subcommand == "const":
                self.put_height_const(int(data))
            elif subcommand == "move":
                self.put_height_move(int(data["duration"]), data["direction"])
            elif subcommand == "preset":
                self.put_height_preset(int(data))
            else:
                raise self.HelpfulError("Bad subcommand")
        else:
            raise self.HelpfulError("Bad command")
        return reply

    def put(self, data):
        try:
            reply = self._put_internal(data)
        except self.HelpfulError as e:
            reply = {"error": e.args[0]}
        return reply

OBJECTS = {"/sarahsdesk": SarahsDesk(), }


class WebControl(http.server.SimpleHTTPRequestHandler):
    def do_PUT(self):
        if self.path in OBJECTS:
            size = int(self.headers.get("content-length"))
            output = OBJECTS[self.path].put(
                json.loads(self.rfile.read(size).decode("UTF-8")))
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            response = json.dumps(output).encode("UTF-8")
            self.wfile.write(response)
        else:
            self.send_response(404)

    def do_GET(self):
        if self.path in OBJECTS:
            output = OBJECTS[self.path].get()
            self.send_response(200)
            self.send_header("Content-type", "application/json")
            self.end_headers()
            response = json.dumps(output).encode("UTF-8")
            self.wfile.write(response)
        else:
            super(WebControl, self).do_GET()

if __name__ == "__main__":
    httpd = http.server.HTTPServer(('', 8080), WebControl)
    httpd.serve_forever()
