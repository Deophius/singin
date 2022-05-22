# Describes the adaptors for our proxy server
import logging, json, requests
try:
    with open('prox.json', 'r', encoding = 'utf-8') as f:
        config = json.load(f)
except FileNotFoundError:
    logging.critical('Logging file not found!')
except json.JSONDecodeError as ex:
    logging.critical('Bad config ' + str(ex.args[0]))

class Adaptor:
    ''' An Adaptor to be applied to the proxy.
    
    An Adaptor should contain the following two member functions:
    1. match(handler): given a handler instance, decides if the URL matches this Adaptor
    2. answer(handler): When the HTTP handler decides to let this adaptor do the
    work, it passes itself into the answer() method. The adaptor is expected to
    finish the whole conversation.
    '''
    def __init__(self):
        pass

    def match(self, handler):
        ''' The match method, to be overriden. '''
        return False
    
    def answer(self, handler):
        ''' The answer method, please override.'''
        pass

class PostRelay(Adaptor):
    ''' This adaptor relays all information to the school server.'''
    def answer(self, h):
        ''' Answers the requests by delegating them to the school server. '''
        # Request body from client
        data = h.rfile.read(int(h.headers['Content-Length']))
        head = {}
        for name in h.headers:
            head[name] = h.headers[name]
        head['User-Agent'] = config['user-agent']
        # Just send them to the server
        try:
            r = requests.post(f'http://{config["server"]}{h.path}',
                headers = head, data = data, timeout = config['timeout'])
        except requests.Timeout as ex:
            logging.warning('Next hop server timed out.')
            h.send_error(502)
            return
        h.send_response(r.status_code)
        for i in r.headers:
            h.send_header(i, r.headers[i])
        h.end_headers()
        h.wfile.write(r.content)

    def match(self, h):
        return h.command == 'POST'

class GetRelay(Adaptor):
    def match(self, h):
        return h.command == 'GET'

    def answer(self, h):
        # There should be no body
        head = dict()
        for name in h.headers:
            head[name] = h.headers[name]
        head['User-Agent'] = config['user-agent']
        try:
            r = requests.get(f'http://{config["server"]}{h.path}', headers = head,
                timeout = config['timeout'])
        except requests.Timeout as ex:
            logging.warning('Next hop server timed out.')
            h.send_error(502)
            return
        h.send_response(r.status_code)
        for i in r.headers:
            h.send_header(i, r.headers[i])
        h.end_headers()
        h.wfile.write(r.content)

class DenyAllElse(Adaptor):
    ''' Deny all connections except those from the loopback interface.'''
    def match(self, h):
        return h.client_address[0] != '127.0.0.1'

    def answer(self, h):
        logging.error(f'A request from {h.client_address[0]} was rejected!')
        h.send_error(403)