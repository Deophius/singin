# Contains the handler of our proxy
from http.server import *
import adapt, logging, sys

class ProxHandler(BaseHTTPRequestHandler):
    ''' The list of adaptors is searched in a first-match, first-go manner.
    If none of the adaptors can handle the request, returns a 404.
    '''
    def __init__(self, req, cli, serv):
        self.server_version = 'Spirit testing'
        self.protocol_version = 'HTTP/1.1'
        self.server = 'Hugehard server'
        # The adaptors contain instances of adaptors, not class names!
        self.adaptors = [adapt.PostRelay()]
        super().__init__(req, cli, serv)
        
    def do_POST(self):
        for adaptor in self.adaptors:
            if adaptor.match(self):
                adaptor.answer(self)
                sys.stderr.flush()
                return
        self.send_error(404, 'Resource not found', '<h1>404 Not found</h1>')
        sys.stderr.flush()

if __name__ == '__main__':
    addr = ('', 80)
    sys.stderr = open('prox.log', 'w', encoding = 'utf-8')
    sys.stdout = sys.stderr
    httpd = HTTPServer(addr, ProxHandler)
    httpd.serve_forever()