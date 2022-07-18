import sys, socket, json

try:
    config = json.load(open('cli.json', encoding = 'utf-8'))
except FileNotFoundError:
    from tkinter.messagebox import showerror
    showerror('No config', 'Please add configuration file cli.json')
    sys.exit(1)

port = config['port']

class LessonInfo:
    def __init__(self, end = ''):
        self.end = end

    def __repr__(self):
        return f'Lesson ending at {self.end}'

# Returns a list of LessonInfo
# If machine name doesn't match, raises RequestFailed
def get_machine_data(machine_name, host):
    req = {
        'command': 'today_info',
        'machine': machine_name
    }
    recv = send_req(host, req, 'get_machine_data')
    return list(map(LessonInfo, recv["end"]))

def send_req(host, req, funcname):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(config['timeout'])
        sock.sendto(bytes(json.dumps(req, ensure_ascii = False), 'utf-8'), (host, port))
        try:
            ans = json.loads(str(sock.recv(config['buffsize']), 'utf-8'))
            if not ans['success']:
                raise RequestFailed(f'{funcname}: {ans["what"]}')
            return ans
        except json.JSONDecodeError:
            print('Invalid reply when sending request:', file=sys.stderr)
            print(req, file = sys.stderr)
            raise RequestFailed(funcname + ': Response format incorrect!')
        except socket.timeout:
            raise RequestFailed(funcname + ": socket timed out")
        except ConnectionResetError:
            raise RequestFailed(funcname + ': Connection was reset! Maybe server is not up?')

class RequestFailed(RuntimeError):
    def __init__(self, msg):
        super().__init__(self, msg)

def report_absent(sessid, host):
    recv = send_req(host, {'command': 'report_absent', 'sessid': sessid}, 'report_absent')
    return recv['name']

def write_record(sessid, host, name):
    send_req(host, {'command': 'write_record', 'sessid': sessid, 'name': name}, 'write_record')

def restart_gs(host):
    send_req(host, {'command': "restart_gs"}, 'restart_gs')