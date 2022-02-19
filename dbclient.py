import time
import os, sys, socket, json

port = 8303

class LessonInfo:
    def __init__(self, start = '', end = ''):
        self.start = start
        self.end = end

    def __repr__(self):
        return f'Lesson {self.start} ~ {self.end}'

def get_broadcast_ip():
    # Configuration variable: the subnet's broadcast ip
    return '192.168.25.255'

# Returns a tuple (ip, list of LessonInfo)
# For example, ('192.168.5.30', [..., ..., ...])
# If machine is not found, returns (None, None)
def get_machine_data(machine_name, broadcast = None):
    if broadcast is None:
        host = get_broadcast_ip()
    else:
        host = broadcast
    print('Broadcast ip is', host)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(10)
        req = {
            'command': 'tell_ip',
            'machine': machine_name
        }
        sock.sendto(bytes(json.dumps(req) + '\n', 'utf-8'), (host, port))
        try:
            raw = ''
            sw_time = time.time()
            while raw == '' and time.time() < sw_time + 10:
                raw = str(sock.recv(1024), 'utf-8')
            print(raw)
            recv = json.loads(raw)
        except socket.timeout:
            print('Unable to receive data in get_machine_data', file = sys.stderr)
            if __name__ == '__main__':
                sys.exit(1)
            else:
                raise
        except json.JSONDecodeError:
            return (None, None)
    check_recv(recv, 'get_machine_data')
    return (recv['ip'], [LessonInfo(s, e) for s, e in zip(recv['start'], recv['end'])])

def send_req(host, req):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(bytes(json.dumps(req, ensure_ascii = False) + '\n', 'utf-8'), (host, port))
        try:
            return json.loads(str(sock.recv(2048), 'utf-8'))
        except json.JSONDecodeError:
            print('Invalid reply when sending request:', file=sys.stderr)
            print(req, file = sys.stderr)
            if __name__ == '__main__':
                sys.exit(1)
            else:
                raise

class RequestFailed(RuntimeError):
    def __init__(self, msg):
        super().__init__(self, msg)

def check_recv(recv, funcname):
    if not recv['success']:
        if __name__ == '__main__':
            print(f'{funcname}:', recv['what'], file = sys.stderr)
            sys.exit(1)
        else:
            raise RequestFailed(f'{funcname}: {recv["what"]}')

def report_absent(sessid, host):
    recv = send_req(host, {'command': 'report_absent', 'sessid': sessid})
    check_recv(recv, 'report_absent')
    return recv['name']

def write_record(sessid, host, name, mode = 'c'):
    recv = send_req(host, {'command': 'write_record', 'sessid': sessid, 'name': name, 'mode': mode})
    check_recv(recv, 'write_record')

def restart_gs(host):
    recv = send_req(host, {'command': "restart_gs"})
    check_recv(recv, 'restart_gs')

def main():
    # Ask for machine input
    machine = ''
    while machine == '':
        machine = input('Please input machine ID (defaults to NJ303): ')
        if machine == '':
            print('Using default value.')
            machine = 'NJ303'
        host, lessons = get_machine_data(machine)
        if host is None and lessons is None:
            print('This machine cannot be found. Maybe the server is not up?')
            machine = ''
    # Now host and lessons are not empty.
    print('Machine IP is', host)
    time.sleep(0.3)
    for num, lesson in enumerate(lessons):
        print(num, lesson)
    if len(lessons) > 10:
        # Use stdin to get the input
        sessid = None
        while sessid is None:
            try:
                sessid = int(input('Please choose a lesson: '))
            except ValueError:
                print('Invalid input, please try again.')
                sessid = None
                continue
            if sessid < 0 or sessid >= len(lessons):
                print('Input out of range, please try again')
                sessid = None
    else:
        # Use choice tool
        sessid = -1 + os.system(
            'choice /m "Please choose a lesson" /c '
            + ''.join(map(str, range(len(lessons))))
        )
    # Now report those who are absent
    absent_names = report_absent(sessid, host)
    print('Those people did not sign in:')
    for i, name in enumerate(absent_names):
        print(i, name)
    # Now read in those who need to sign in
    while True:
        print('Who should sign in? Please specify their number.')
        sign_ids = None
        while sign_ids == None:
            try:
                sign_ids = list(map(int, input().split()))
            except ValueError:
                print('Incorrect format, please try again.')
                sign_ids = None
            if list(filter(lambda x: x < 0 or x >= len(absent_names), sign_ids)) != []:
                print('There are out of bound values, please try again.')
                sign_ids = None
        print('Please confirm:')
        for i in sign_ids:
            print(absent_names[i], end=' ')
        print()
        rc = os.system('choice /c RCNQ /m "R:Randomly signin, C:current time signin, N: abort and'
            + ' repick, Q: abort and quit system"')
        if rc == 3:
            continue
        elif rc == 4:
            print('Goodbye!')
            sys.exit(0)
        else:
            write_record(sessid, host, [absent_names[i] for i in sign_ids], 'r' if rc == 1 else 'c')
        break
    rc = os.system('choice /m "Should I restart terminal?"')
    if rc == 1:
        restart_gs(host)
    print('Goodbye')

if __name__ == '__main__':
    main()