from json.decoder import JSONDecodeError
import subprocess, socketserver, time, json, os

# Configuration variable: path to GS.Terminal.Smartboard.exe
gs_path = 'D:/杭二中/SmartBoardHost 3.4.3.106全包/GS.Terminal.SmartBoard.exe'

class SpiritTCPHandler(socketserver.StreamRequestHandler):
    ''' The request handling class for our server
    In signin.py, sessid is called time
    In C++ parts, sessid is called dk_curr
    
    It supports two requests, both in JSON format:
    {"command": "report_absent", "sessid": 1}
        -> {"success": true, "cardnum": ["D1234567", "12345678", ...]} on success
        -> {"success": false, "what": "error description"} on failure

    {"command": "write_record", "cardnum": ["ABC12345", ...], "sessid":1, ["mode": "c" or "r"],
        ["start": "17:15:00", "end": "17:55:00"]}
        -> {"success": true} on success
        -> {"success": false, "what": "error description"} on failure
        Mode c: current time, pause for 0.7s between two commits
        Mode r: Randomly generates time.
        "start", "end": timestr denoting the start and end of a DK session.
        If mode is empty, then defaults to "c" for backward compatibility.

    {"command": "restart_gs"} (Restarts GS.Terminal)
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

    def report_absent(self, req):
        if 'sessid' not in req:
            self.write_str('{"success": false, "what": "No session id specified via sessid!"}')
            return
        result = {"success": True}
        try:
            p = subprocess.run(["report_absent"], capture_output=True, text=True, input=str(req['sessid']))
            assert p.returncode == 0
            result['cardnum'] = p.stdout.strip('\n').split('\n')
        except:
            result['what'] = 'Failed to invoke report_absent'
            result['success'] = False
        self.write_str(json.dumps(result))

    def write_record(self, req):
        result = {"success": True}
        try:
            if req['mode'] == 'c':
                prog_in = str(req['sessid']) + ' c\n' + ' '.join(req['cardnum'])
            elif req['mode'] == 'r':
                prog_in = '{} r {} {}\n'.format(req['sessid'], req['start'], req['end']) + ' '.join(req['cardnum'])
            else:
                raise ValueError
            p = subprocess.run(["write_record"], input = prog_in, text=True)
            # print(p.returncode)
            # print(p.stderr)
        except KeyError as ex:
            result['success'] = False
            result['what'] = 'request did not include ' + ex.args[0]
        except ValueError:
            result['success'] = False
            result['what'] = 'Unknown mode: ' + str(req['mode'])
        # except:
        #    result['what'] = 'Failed to invoke write_record'
        #   result['success'] = False
        self.write_str(json.dumps(result))

    def restart_gs(self, req):
        # Ignore return value because this is to ensure GS is closed
        os.system('taskkill /im GS.Terminal.SmartBoard.exe /f /t > NUL 2> NUL')
        if os.system('start cmd /c' + gs_path) == 0:
            self.write_str('{"success": true}')
        else:
            self.write_str('{"success": false", "what": "Failed to start GS.Terminal"}')
    
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
        if req['command'] == 'report_absent':
            self.report_absent(req)
        elif req['command'] == 'write_record':
            if 'mode' not in req:
                req['mode'] = 'c'
            self.write_record(req)
        elif req['command'] == 'restart_gs':
            self.restart_gs(req)
        else:
            self.write_str('{"success": false, "what": "Unknown command!"}')

if __name__ == '__main__':
    host = 'localhost'
    port = 8303
    print('dbman version 2.0 listening on port', port)
    with socketserver.TCPServer((host, port), SpiritTCPHandler) as server:
        server.serve_forever()
