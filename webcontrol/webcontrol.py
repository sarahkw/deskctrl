#!/usr/bin/env python3

import http.server
import json
import serial
import struct

class Protocol(object):
    @staticmethod
    def _encode(command, argument):
        return struct.pack("<HH", command, argument)
    
    @staticmethod
    def getHeight():
        return Protocol._encode(1, 0)

    @staticmethod
    def setHeight(height):
        return Protocol._encode(2, height)

    @staticmethod
    def moveUp(duration):
        return Protocol._encode(3, duration)

    @staticmethod
    def moveDown(duration):
        return Protocol._encode(4, duration)

class SarahsDesk(object):
    def __init__(self):
        self.ser = serial.Serial("/dev/ttyACM0", 9600)

    class HelpfulError(Exception):
        pass

    def put_height_const(self, height):
        print("Set height to {}".format(height))
        self.ser.write(Protocol.setHeight(height))

    def put_height_move(self, duration, direction):
        assert direction in ("up", "down")
        print("Move for {} ms towards {}".format(duration, direction))
        if direction == "up":
            self.ser.write(Protocol.moveUp(duration))
        elif direction == "down":
            self.ser.write(Protocol.moveDown(duration))

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
            elif subcommand == "percent":
                MIN = 242
                MAX = 498
                self.put_height_const(int(int(data) / 100 * (MAX-MIN) + MIN))
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
        elif self.path == "/clip.html":
            # Whitelist
            super(WebControl, self).do_GET()
        else:
            self.send_response(404)


if __name__ == "__main__":
    httpd = http.server.HTTPServer(('', 8080), WebControl)
    httpd.serve_forever()
