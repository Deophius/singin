from json.decoder import JSONDecodeError
import subprocess, socketserver, time, json

class SpiritTCPHandler(socketserver.StreamRequestHandler):
    ''' The request handling class for our server
    In signin.py, sessid is called time
    In C++ parts, sessid is called dk_curr
    
    It supports two requests, both in JSON format:
    {"command": "report_absent", "sessid": 1}
        -> {"success": true, "cardnum": ["D1234567", "12345678", ...]} on success
        -> {"success": false, "what": "error description"} on failure

    {"command": "write_record", "cardnum": ["ABC12345", ...], "sessid":1}
        -> {"success": true} on success
        -> {"success": false, "what": "error description"} on failure

    Unrecognized format will result in:
        {"success": false, "what": "Unrecognized format!"}

    If no "command" is found or the contents are not unrecognized, results in:
        {"success": false, "what": "Unknown command!"}

    Note that Python requires a \n at the end of the request.
    '''
    def write_str(self, s):
        self.wfile.write(bytes(s, 'utf-8'))
    
    def handle(self):
        ''' Handles a request'''
        self.data = str(self.rfile.readline().strip(), 'utf-8')
        print(time.asctime() + ': Connection from', self.client_address[0])
        print('    ', self.data)
        req = None
        try:
            req = json.loads(self.data)
        except JSONDecodeError:
            self.write_str('{"success": false, "what": "Unrecognized format!"}')
            return
        if "command" not in req:
            self.write_str('{"success": false, "what": "No command!"}')
            return
        if 'sessid' not in req:
            self.write_str('{"success": false, "what": "No session id specified via sessid!"}')
            return
        if req['command'] == 'report_absent':
            result = {"success": True}
            try:
                p = subprocess.run(["report_absent"], capture_output=True, text=True, input=str(req['sessid']))
                assert p.returncode == 0
                result['cardnum'] = p.stdout.strip('\n').split('\n')
            except:
                result['what'] = 'Failed to invoke report_absent'
                result['success'] = False
            self.write_str(json.dumps(result))
        elif req['command'] == 'write_record':
            result = {"success": True}
            try:
                p = subprocess.run(["write_record"], input = str(req['sessid']) + '\n' + ' '.join(req['cardnum']), text=True)
                assert p.returncode == 0
            except KeyError:
                result['success'] = False
                result['what'] = 'request did not include cardnum'
            except:
                result['what'] = 'Failed to invoke write_record'
                result['success'] = False
            self.write_str(json.dumps(result))
        else:
            self.write_str('{"success": false, "what": "Unknown command!"}')

if __name__ == '__main__':
    host = 'localhost'
    port = 8303
    with socketserver.TCPServer((host, port), SpiritTCPHandler) as server:
        server.serve_forever()
