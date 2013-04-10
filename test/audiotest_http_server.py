#!/usr/bin/python

# test with:
# curl -d "param1=value1&param2=value2" http://localhost:4444

#---------------------------------------------------------------
import itertools
import os
import re

def unique_file_name(file):
    ''' Append a counter to the end of file name if 
    such file allready exist.'''
    if not os.path.isfile(file):
        # do nothing if such file doesn exists
        return file 
    # test if file has extension:
    if re.match('.+\.[a-zA-Z0-9]+$', os.path.basename(file)):
        # yes: append counter before file extension.
        name_func = \
            lambda f, i: re.sub('(\.[a-zA-Z0-9]+)$', '_%i\\1' % i, f)
    else:
        # filename has no extension, append counter to the file end
        name_func = \
            lambda f, i: ''.join([f, '_%i' % i])
    for new_file_name in \
            (name_func(file, i) for i in itertools.count(1)):
        if not os.path.exists(new_file_name):
            return new_file_name
#---------------------------------------------------------------

from sys import version as python_version
from cgi import parse_header, parse_multipart
import base64

if python_version.startswith('3'):
    from urllib.parse import parse_qs
    from http.server import BaseHTTPRequestHandler, HTTPServer
else:
    from cgi import parse_qs
    from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

class RequestHandler(BaseHTTPRequestHandler):

    def parse_POST(self):
        ctype, pdict = parse_header(self.headers['content-type'])
        if ctype == 'multipart/form-data':
            postvars = parse_multipart(self.rfile, pdict)
        elif ctype == 'application/x-www-form-urlencoded':
            length = int(self.headers['content-length'])
            postvars = parse_qs(
                    self.rfile.read(length), 
                    keep_blank_values=1)
        else:
            postvars = {}
        return postvars

    def do_POST(self):
        # Get the content
        postvars = self.parse_POST()
        if (postvars):
            print postvars

            # Send a basic response
            self.send_response(200)
            self.end_headers()
            self.wfile.write("POST vars received.")
        else:
            content_len = int(self.headers.getheader('content-length'))
            post_body = self.rfile.read(content_len)

            # Base64 decode and save to a file
            dataPath = os.path.join("data", self.path[1:])
            if not os.path.exists(dataPath):
                os.makedirs(dataPath)
            aLawFilePath = unique_file_name(os.path.join(dataPath, 
                                        "audiobeep.alaw"))
            wavFilePath = aLawFilePath.replace("alaw", "wav")
            file = open(aLawFilePath, "wb")
            file.write(base64.b64decode(post_body))
            file.close()
            os.system("sox -r 16k -t raw -e a-law %s %s" % (aLawFilePath, 
                      wavFilePath))

            # Send a basic response
            self.send_response(200)
            self.end_headers()
            self.wfile.write("Audio received.")


if __name__ == '__main__':
    server = HTTPServer(('', 4444), RequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    server.server_close()

