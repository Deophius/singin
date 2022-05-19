# Describes the adaptors for our proxy server
import requests

class Adaptor:
    ''' An Adaptor to be applied to the proxy.
    
    An Adaptor should contain the following two member functions:
    1. match(url): given a URL, decides if the URL matches this Adaptor
    2. answer(handler): When the HTTP handler decides to let this adaptor do the
    work, it passes itself into the answer() method. The adaptor is expected to
    finish the whole conversation.
    '''
    def __init__(self):
        pass

    def match(self, url):
        ''' The match method, to be overriden. '''
        return False
    
    def answer(self, handler):
        ''' The answer method, please override.'''
        pass

class RelayPost(Adaptor):
    ''' This adaptor relays all information to the school server.'''
    # FIXME: Try to configurationize this part.
    def match(self, url):
        # Match all
        return True

    def answer(self, h):
        ''' Answers the requests by delegating them to the school server. '''
        # Request body from client
        data = h.rfile.read(int(h.headers['Content-Length']))
        print(data)
        # Just send them to the server
        r = requests.post('http://192.168.16.85' + h.path, data = data)
        h.send_response(r.status_code)
        for i in r.headers:
            h.send_header(i, r.headers[i])
        h.end_headers()
        h.wfile.write(r.content)