import signin
from requests import Session
from time import asctime, localtime
import os, sys, socket, json

host = 'localhost'
port = 8303

def get_card_map(sess, time, kid):
    ''' Returns a list of (cardnum, name) '''
    if time not in (0, 1, 2):
        raise RuntimeError('Wrong time argument!')
    ct = localtime()
    head = {'Expect': '100-continue', 'user-agent': signin.user_agent}
    body = {
        'ID': kid,
        'date': '',
        'isloadfacedata': False,
        'faceversion': 2
    }
    body['date'] = signin.get_end_time(ct, time)
    resp = signin.send_req(sess, head, body, signin.url_student_new)
    try:
        return [(s["CardID"], s["StudentName"]) for s in resp["result"]["students"]]
    except KeyError:
        print(asctime(), ": Bad server reply in get_card_map()", file=sys.stderr)
        sys.exit(1)

def report_absent():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        sock.sendall(bytes('{"command": "report_absent"}\n', 'utf-8'))
        recv_raw = str(sock.recv(1024), 'utf-8')
    recv_data = json.loads(recv_raw)
    if recv_data['success']:
        return recv_data['cardnum']
    else:
        print(asctime(), ': dbman server has bad news', file=sys.stderr)
        print('    ', recv_data['what'], file=sys.stderr)
        sys.exit(1)

sess = Session()
dk_time = os.system('choice /c 012 /m "0 for morning, 1 for afternoon, 2 for night"') - 1
print('User specified:', dk_time)
print(asctime(), ": POSTing KID")
kid = signin.get_kid(sess, dk_time)
print('kid is:', kid)
card_map = get_card_map(sess, dk_time, kid)
absent_cards = report_absent()
absent_names = [s[1] for s in card_map if s[0] in absent_cards]
print('Those people did not DK:')
for enum, name in enumerate(absent_names):
    print(enum, name)