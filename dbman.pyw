from json.decoder import JSONDecodeError
import subprocess, socketserver, time, json, os

try:
    config = json.load(open('man.json', 'r', encoding = 'utf-8'))
except FileNotFoundError:
    from tkinter.messagebox import showerror
    showerror('Configuration not found', 'Please create the configuration file in cwd.')

class SpiritUDPHandler(socketserver.DatagramRequestHandler):
    ''' The request handling class for our server
    In C++ parts, sessid is called dk_curr

    It supports six requests, all in JSON format:
    {"command": "report_absent", "sessid": 1}
        -> {"success": true, "name": ["xxx", "xxx", ...]} on success
        -> {"success": false, "what": "error description"} on failure

    {"command": "write_record", "name": ["xxx", ...], "sessid":1, ["mode": "c" or "r"]}
        -> {"success": true} on success
        -> {"success": false, "what": "error description"} on failure
        Mode c: current time, pause for 0.7s between two commits
        Mode r: Randomly generates time.
        If mode is empty, then defaults to "c" for backward compatibility.

    {"command": "restart_gs"} (Restarts GS.Terminal)
        -> {"success": true} on success
        -> {"success": false, "what": "error description"} on failure

    {"command": "tell_ip", "machine": "NJ303"} (Look for the machine's IP)
        -> {"success": true, "ip": "192.168.xxx.xxx",start:[<timestrs>], "end": [<timestr>]}
        -> {"success": false, "what": ...} if error occurs and this is the correct machine

    {"command": "get_watchdog"} (Gets a list of lessons that have watchdogs set)
        -> {"success": true, "start": [<timestrs>], "end": [<timestrs>]}
        -> {"success": false, "what": ...} if error occurs.

    {"command": "set_watchdog", "start": [<timestrs>], "end": [<timestrs>]}
    (Sets the watchdog list)
        -> {"success": true}
        -> {"success": false, "what": "error description"} if error occurs

    Unrecognized format will result in:
        {"success": false, "what": "Unrecognized format!"}

    If no "command" is found or the contents are not unrecognized, results in:
        {"success": false, "what": "Unknown command!"}

    Note that Python requires a \n at the end of the request.
    '''
    def write_object(self, o):
        self.wfile.write(bytes(json.dumps(o), 'utf-8'))
        with open('dbman.log', 'a', encoding = 'utf-8') as f:
            print(time.asctime(), ": Replied with", file = f)
            print('    ', json.dumps(o, ensure_ascii = False), file = f)

    def report_absent(self, req):
        if 'sessid' not in req:
            self.write_object({
                'success': False,
                'what': "No session id specified via sessid!"
            })
            return
        result = {"success": True}
        try:
            p = subprocess.run(
                ["report_absent"],
                capture_output=True,
                text=True,
                input = f'{config["dbname"]} {config["passwd"]} {req["sessid"]}',
                encoding='utf-8',
                creationflags = subprocess.CREATE_NO_WINDOW,
                check = True
            )
            result['name'] = p.stdout.strip('\n').split()
        except subprocess.CalledProcessError as ex:
            result['what'] = 'Failed to invoke report_absent\n' + ex.stderr
            result['success'] = False
        self.write_object(result)

    def write_record(self, req):
        if 'mode' not in req:
            req['mode'] = 'c'
        result = {"success": True}
        try:
            if req['mode'] not in ('c', 'r'):
                raise ValueError
            subprocess.run(
                ["write_record"],
                input = f"{config['dbname']} {config['passwd']} {req['sessid']} {req['mode']}\n"
                    + ' '.join(req['name']),
                text = True,
                encoding='utf-8',
                creationflags = subprocess.CREATE_NO_WINDOW,
                check = True
            )
        except KeyError as ex:
            result['success'] = False
            result['what'] = 'request did not include ' + ex.args[0]
        except ValueError:
            result['success'] = False
            result['what'] = 'Unknown mode: ' + str(req['mode'])
        except subprocess.CalledProcessError as ex:
            result['success'] = False
            result['what'] = ex.stderr
        except:
            result['what'] = 'Failed to invoke write_record'
            result['success'] = False
        self.write_object(result)

    def restart_gs(self):
        # Ignore return value because this is to ensure GS is closed
        subprocess.run(
            'taskkill /im GS.Terminal.SmartBoard.exe /f /t'.split(),
            creationflags = subprocess.CREATE_NO_WINDOW,
            stdout = subprocess.DEVNULL,
            stderr = subprocess.DEVNULL,
        )
        try:
            os.startfile(config['gs_path'])
            self.write_object({"success": True})
        except OSError:
            self.write_object({
                "success": False,
                "what": "Failed to start GS.Terminal"
            })

    def tell_ip(self, req):
        machine_id = None
        start, end = [], []
        result = {"success": True}
        try:
            p = subprocess.run(
                ['today_info'],
                capture_output = True,
                encoding = 'utf-8',
                text = True,
                creationflags = subprocess.CREATE_NO_WINDOW,
                input = f"{config['dbname']} {config['passwd']}"
            )
            assert p.returncode in (0, 1)
            machine_id = p.stdout.strip().split('\n')[0]
            assert p.returncode == 0
            for timepair in p.stdout.strip().split('\n')[1:]:
                start.append(timepair.split()[0])
                end.append(timepair.split()[1])
        except AssertionError:
            result['success'] = False
            result['what'] = p.stderr
        except:
            result['success'] = False
            result['what'] = 'Unknown error calling today_info'
        try:
            if req['machine'] == machine_id:
                # The message is for us
                if not result['success']:
                    # Failed and this is the correct machine
                    self.write_object(result)
                    return
                from socket import gethostbyname, gethostname
                result['ip'] = gethostbyname(gethostname())
                result['start'] = start
                result['end'] = end
                self.write_object(result)
        except KeyError as ex:
            self.write_object({
                "success": False,
                "what": "No {} specified".format(ex.args[0])
            })

    def get_watchdog(self):
        try:
            self.write_object({
                'success': True,
                'start': [x[0] for x in config['watchdog']],
                'end': [x[1] for x in config['watchdog']]
            })
        except KeyError as ex:
            self.write_object({
                'success': False,
                'what': f"Missing {ex.args[0]} in config file."
            })

    def set_watchdog(self, req):
        try:
            if len(req['start']) != len(req['end']):
                raise ValueError("Sizes of start and end don't match!")
            config['watchdog'] = list(zip(req['start'], req['end']))
            with open('man.json', 'w', encoding = 'utf-8') as config_file:
                json.dump(config, config_file, ensure_ascii = False, indent = 4)
            self.write_object({
                'success': True
            })
        except ValueError as ex:
            self.write_object({
                'success': False,
                'what': ex.args[0]
            })
        except KeyError as ex:
            self.write_object({
                'success': False,
                'what': f'Missing {ex.args[0]} in request'
            })

    def handle(self):
        ''' Handles a request'''
        self.data = str(self.rfile.readline().strip(), 'utf-8')
        with open('dbman.log', 'a', encoding = 'utf-8') as logfile:
            print(time.asctime() + ': Connection from', self.client_address[0], file = logfile)
            print('    ', self.data, file = logfile)
        req = None
        try:
            req = json.loads(self.data)
        except JSONDecodeError:
            self.write_object({"success": False, "what": "Unrecognized format!"})
            return
        if "command" not in req:
            self.write_object({"success": False, "what": "No command!"})
            return
        if req['command'] == 'report_absent':
            self.report_absent(req)
        elif req['command'] == 'write_record':
            self.write_record(req)
        elif req['command'] == 'restart_gs':
            self.restart_gs()
        elif req['command'] == 'tell_ip':
            self.tell_ip(req)
        elif req['command'] == 'get_watchdog':
            self.get_watchdog()
        elif req['command'] == 'set_watchdog':
            self.set_watchdog(req)
        else:
            self.write_object({"success": False, "what": "Unknown command!"})

if __name__ == '__main__':
    from socket import gethostbyname, gethostname
    host = gethostbyname(gethostname())
    port = config["port"]
    with open('dbman.log', 'w', encoding = 'utf-8') as logfile:
        print(time.asctime(), ': dbman version', config['version'], 'listening on port', port, file = logfile)
        os.system('taskkill /im LockMouse.exe /f /t > NUL 2> NUL')
        print(time.asctime(), ': called taskkill to terminate LockMouse', file = logfile)
        if 'watchdog' in config and 'url_student_new' in config and 'user_agent' in config:
            os.startfile('watchdog.pyw')
            print(time.asctime(), ': launched watchdog.', file = logfile)
    with socketserver.UDPServer((host, port), SpiritUDPHandler) as server:
        server.serve_forever()
