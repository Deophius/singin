import json
import os
import socket
import sys
from random import randrange
from time import localtime, mktime, sleep, struct_time

from requests import Request, Session
from requests.exceptions import ReadTimeout

# Constants: the URLs
url_class = 'http://192.168.16.85//Services/SmartBoard/SmartBoardSingInClass/json'
url_student_new = 'http://192.168.16.85//Services/SmartBoard/SmartBoardLoadSingInStudentNew/json'
url_course_state = 'http://192.168.16.88//Services/SmartBoard/SmartBoardUpdateSingCourseState/json'
url_leave_info = 'http://192.168.16.88//Services/SmartBoard/SmartBoardGetStudentLeaveInfo/json'
url_card_info = 'http://192.168.16.250//Services/SmartBoard/SmartBoardUploadStudentCardInfo/json'
# Constant: the user-agent to use
user_agent = 'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/86.0.4240.198 Safari/537.36'
# Name of process
process_name = 'GS.Terminal.SmartBoard.exe'
# ID of people who don't need to dk
no_dk = { '9b37b535-5103-4656-8a76-3ed4aa736736' }
# IP address of the card machine
host = '192.168.5.143'
# The port to use for listening
port = 8512

def make_date_str(ct):
    return str(ct.tm_year) + '-' + "%02d" % ct.tm_mon + '-' + "%02d" % ct.tm_mday

def get_end_time(ct, time):
    ret = make_date_str(ct)
    if time == 0:
        ret += 'T07:20:00'
    elif time == 1:
        ret += 'T18:00:00'
    elif time == 2:
        ret += 'T20:25:00'
    return ret

def send_req(sess, headers, body, url):
    ''' Sends the request in the specified way to the server.'''
    headers['Content-Type'] = 'application/json'
    prep = Request(
        'POST',
        url=url,
        headers = headers,
        data = json.dumps(body, ensure_ascii = False, separators = (',', ':')).encode('utf-8')
    ).prepare()
    print(prep.body)
    try:
        return sess.send(prep, timeout = 10).json()
    except ReadTimeout:
        return {}

def get_kid(sess, time):
    ''' Gets KID of a sign in session.
        sess is the session to be used.
        Time refers to morning, afternoon and night with 0, 1, 2
        Returns KID if it exists. Otherwise, returns None
    '''

    if time not in (0, 1, 2):
        raise RuntimeError('Wrong time input argument!')
    ct = localtime()
    head = {'Expect': '100-continue', 'Connection': 'keep-alive', 'user-agent': user_agent}
    body = {
        'tCode': "NJ303",
        'date': make_date_str(ct) + 'T00:00:00'
    }
    resp = send_req(sess, head, body, url_class)
    try:
        for lesson in resp['result']:
            start_time = int(lesson['DKStartTime'][11:13])
            if time == 0 and start_time == 6: return lesson['KeChengAnPaiID']
            if time == 1 and start_time == 17: return lesson['KeChengAnPaiID']
            if time == 2 and start_time == 20: return lesson['KeChengAnPaiID']
    except KeyError:
        print('bad reply in get_kid', file = sys.stderr)
    # Not found, must return None
    return None

def get_stuinfo(sess, time, kid):
    '''
    POSTs SingInStudentNew to get the student ids.
    Returns (stu_id, invalid) in a list
    time and kid are required for the POST request.
    '''
    if time not in (0, 1, 2):
        raise RuntimeError('Wrong time argument!')
    ct = localtime()
    head = {'Expect': '100-continue', 'user-agent': user_agent}
    body = {
        'ID': kid,
        'date': '',
        'isloadfacedata': True,
        'faceversion': 2
    }
    body['date'] = get_end_time(ct, time)
    resp = send_req(sess, head, body, url_student_new)
    try:
        return [(s["StudentID"], s["Invalid"]) for s in resp["result"]["students"]]
    except KeyError:
        print('Bad return value in get_stuinfo', file = sys.stderr)
        return []

def send_state4(sess, kid):
    '''
    POSTs UpdateSingCourse with state: 4
    '''
    head = {'Expect': '100-continue', 'Connection': 'keep-alive', 'user-agent': user_agent}
    body = {
        'tCode': 'NJ303',
        'kid': str(kid),
        'state': 4
    }
    resp = send_req(sess, head, body, url_course_state)
    if 'result' in resp and resp['result'] == True:
        pass
    else:
        print('Bad reply from server in start_dk()', file = sys.stderr)

def send_state5(sess, kid):
    ''' POSTs UpdateSingCourse with state of 5 '''
    head = {'Expect': '100-continue', 'Connection': 'keep-alive', 'user-agent': user_agent}
    body = {
        'tCode': 'NJ303',
        'kid': str(kid),
        'state': 5
    }
    resp = send_req(sess, head, body, url_course_state)
    if 'result' in resp and resp['result'] == True:
        pass
    else:
        print('Bad reply from server in end_dk()', file = sys.stderr)

def get_leave_info(sess, kid, ids_left):
    '''
    POSTs leave_info page with ids_left as parameter.
    ids_left should be a list of people who don't need to DK and those not at school.
    '''
    head = {'Expect': '100-continue', 'user-agent': user_agent}
    ct = localtime()
    body = {
        'courseid': kid,
        'coursedate': make_date_str(ct) + 'T00:00:00',
        'studentids': ids_left
    }
    resp = send_req(sess, head, body, url_leave_info)
    if 'result' not in resp:
        print('Bad server reply in GetStudentLeaveInfo', file = sys.stderr)

def make_mili():
    mili = str(randrange(1, 10000000))
    # Make it 7 bits long
    while len(mili) != 7:
        mili = '0' + mili
    mili = mili.rstrip('0')
    return mili

def upload_card_info(sess, kid, ids_left, all_stu, time):
    '''
    Uploads card info (fake)
    ids_left is a list of people who don't need to sign in, which is wwc + (not at school)
    all_stu is a list of IDs of all the students.
    time should be 0, 1, 2
    '''
    if time not in (0, 1, 2):
        raise RuntimeError('Wrong time input argument!')
    ct = localtime()
    head = {'Expect': '100-continue', 'user-agent': user_agent}
    body = {
        'tCode': 'NJ303',
        'coursedate': make_date_str(ct) + "T00:00:00",
        'courseid': str(kid),
        'datas': []
    }
    for id in all_stu:
        # Populate datas with time concerning info
        # sort into three cases
        card_time = ''
        if time == 0:
            minute = randrange(30, 80)
            hour = 6 + minute // 60
            minute %= 60
            second = randrange(0, 60)
            mili = make_mili()
            card_time = make_date_str(ct) + 'T%02d:%02d:%02d.' % (hour, minute, second) + mili
        elif time == 1:
            minute = randrange(15, 55)
            second = randrange(0, 60)
            mili = make_mili()
            card_time = make_date_str(ct) + 'T17:%02d:%02d.' % (minute, second) + mili
        else:
            minute = randrange(10, 17)
            second = randrange(0, 60)
            mili = make_mili()
            card_time = make_date_str(ct) + 'T20:%02d:%02d.' % (minute, second) + mili
        body['datas'].append({
            'cardtime': card_time,
            'studentid': id,
            'facevalue': 0.0,
            'tag': '正常'
        })
    # Change those in ids_left to mianqian
    no_card = make_date_str(ct)
    if time == 0:
        no_card += 'T07:21:00'
    elif time == 1:
        no_card += 'T17:56:00'
    else:
        no_card += 'T20:21:00'
    for stu in body['datas']:
        if stu['studentid'] in ids_left:
            # Sort into 3 cases
            stu['cardtime'] = no_card
            stu['tag'] = '免签'
    # part of teacher
    body['teacherSign'] = {
        'teacherTime': no_card,
        "teacherID": "6c554dbc-5900-47e0-a251-93dc1d6d93f0",
        "tag": "未到"
    }
    # Now send them up to the server
    resp = send_req(sess, head, body, url_card_info)
    if 'result' in resp and resp['result'] == True:
        pass
    else:
        print('Bad server reply in upload_card_info()', file = sys.stderr)

def make_time_struct(ct, hour, minute, second):
    return struct_time((
        ct.tm_year,
        ct.tm_mon,
        ct.tm_mday,
        hour,
        minute,
        second,
        ct.tm_wday,
        ct.tm_yday,
        ct.tm_isdst
    ))

def handle_dk(time):
    ''' time is 0, 1 or 2 '''
    ct = localtime()
    if time == 0:
        start_time = make_time_struct(ct, 6, 20, 0)
        end_time = make_time_struct(ct, 7, 20, 0)
    elif time == 1:
        start_time = make_time_struct(ct, 17, 15, 0)
        end_time = make_time_struct(ct, 17, 55, 0)
    elif time == 2:
        start_time = make_time_struct(ct, 20, 5, 0)
        end_time = make_time_struct(ct, 20, 20, 0)
    else:
        raise RuntimeError('time is expected to be in (0, 1, 2)')
    if ct > end_time:
        print('DK {} has already passed.'.format(time))
        return
    sess = Session()
    kid = get_kid(sess, time)
    print('Done POSTing SingInClass. KID is', kid)
    if kid == None:
        print('Assuming that this lesson does not exist, entering next loop.')
        return
    os.system('taskkill /f /im {}'.format(process_name))
    print('Successfully killed GS terminal exe, without checking for return value')
    if start_time <= ct <= end_time:
        print('In the middle of DK', time)
        print('Skipping state 4 POST')
    else:
        # Then ct < start_time
        sleep(mktime(start_time) - mktime(localtime()))
        # Now is the time to send signal 4
        send_state4(sess, kid)
        print('Sent state 4 POST')
    sleep(mktime(end_time) - mktime(localtime()))
    send_state5(sess, kid)
    print('Sent state 5 POST #1')
    stuinfo = get_stuinfo(sess, time, kid)
    print('Successfully got stuinfo')
    allstu = [s[0] for s in stuinfo]
    left = list({ s[0] for s in stuinfo if s[1] }.union(no_dk))
    del stuinfo
    sleep(max(0, mktime(end_time) + 15 - mktime(localtime())))
    get_leave_info(sess, kid, left)
    print('Successfully POSTed leaveinfo')
    sleep(max(0, mktime(end_time) + 20 - mktime(localtime())))
    upload_card_info(sess, kid, left, allstu, time)
    print('Successfully POSTed card info')
    send_state5(sess, kid)
    print('Sent state 5 POST #2')
    print('DK {} stops. Restarting smartboard'.format(time))

if __name__ == '__main__':
    # Wait to be activated by a socket
    soc = socket.socket()
    soc.bind((host, port))
    soc.listen(1)
    conn, addr = soc.accept()
    # Now start
    soc.close()
    conn.close()
    print('Trigger pressed by', addr)
    for i in (0, 1, 2):
        handle_dk(i)
