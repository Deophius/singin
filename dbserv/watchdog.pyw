# Watch dog functionality on the server, logs are written to watchdog.log.
import time, subprocess, sys
import os
import os.path

class LessonInfo:
    def __init__(self, start = '', end = ''):
        self.start = start.strip()
        self.end = end.strip()
        
    def __repr__(self):
        return f'Lesson {self.start} ~ {self.end}'
    
    @staticmethod
    def to_sec_cnt(timestr):
        h, m, s = map(int, timestr.split(':'))
        return 3600 * h + 60 * m + s
    
    def __eq__(self, value):
        ''' Returns self == value. Only if they have the same start and end '''
        return self.start == value.start and self.end == value.end

try:
    import json
    config = json.load(open('man.json', 'r', encoding = 'utf-8'))
except FileNotFoundError:
    from tkinter.messagebox import showerror
    showerror('Configuration not found', 'Please create the configuration file in cwd.')
    
def read_today_info():
    ''' This function will return a list of all the lessons in the DB. '''
    try:
        p = subprocess.run(
            ['today_info'],
            capture_output = True,
            encoding = 'utf-8',
            text = True,
            creationflags = subprocess.CREATE_NO_WINDOW,
            input = f"{config['dbname']} {config['passwd']}"
        )
    except subprocess.CalledProcessError as ex:
        # For some reason, nothing in DB.
        # Doing nothing is absolutely safe.
        return []
    ans = []
    for line in p.stdout.strip().split('\n')[1:]:
        s, e = line.split(' ')
        ans.append(LessonInfo(s, e))
    return ans

def worker(lesson : LessonInfo):
    ''' Does the actual work.
    
    First, posts the server for information.
    Then, sign in for those who are absent and not invalid.
    '''
    from requests import Request, Session
    import requests.exceptions
    flag = False
    for i in read_today_info():
        if LessonInfo.to_sec_cnt(i.end) == LessonInfo.to_sec_cnt(lesson.end):
            flag = True
            break
    if not flag:
        write_log(lesson, 'not in DB.')
        return
    write_log('Start watchdog for', lesson)
    try:
        url_student_new = config['url_student_new']
        # The user agent to use
        user_agent = config['user_agent']
    except KeyError as ex:
        write_log(ex.args, 'not found in config file!')
        return
    headers = {'User-Agent': user_agent, 'Content-Type': 'application/json'}
    sess = Session()
    # Now, get the string
    try:
        p = subprocess.run(
            ['sess_req'],
            input = f"{config['dbname']} {config['passwd']} {lesson.start} {lesson.end}\n",
            text = True,
            encoding = 'utf-8',
            creationflags = subprocess.CREATE_NO_WINDOW,
            capture_output = True
        )
        if p.returncode != 0:
            raise ValueError(p.stderr)
    except ValueError as ex:
        # Shocking news, it failed, just log it and exit
        write_log('Watchdog for', lesson, 'failed because', ex.args[0])
        return
    prep = Request('POST', url = url_student_new, headers = headers, data = p.stdout).prepare()
    try:
        req = sess.send(prep, timeout = 10).json()
        # Debug purposes only!
        # with open('stuinfo.json', encoding = 'utf-8') as f:
        #    req = json.load(f)
    except requests.exceptions.RequestException as ex:
        print(time.asctime())
        print('Watchdog ', lesson, 'failed because of networking error.')
        print('exception was', type(ex), ex.args, flush = True)
        return
    try:
        invalid_names = {stu['StudentName'] for stu in req['result']['students'] if stu['Invalid']}
        write_log(len(invalid_names), 'are invalid.')
    except KeyError as ex:
        write_log('Missing', ex.args[0], 'exiting on', lesson)
        return
    # Now read those who are absent
    try:
        # The index can be sought in the output of today_info
        p = subprocess.run(
            ['report_absent'],
            capture_output = True,
            encoding = 'utf-8',
            text = True,
            creationflags = subprocess.CREATE_NO_WINDOW,
            input = f"{config['dbname']} {config['passwd']} {read_today_info().index(lesson)}"
        )
        if p.returncode != 0:
            raise RuntimeError(p.stderr)
        absent_names = set(p.stdout.split())
        write_log(len(absent_names), 'are absent.')
    except RuntimeError as ex:
        write_log('Report_absent failed with stderr', ex.args[0], sep = '\n')
        return
    need_sign = absent_names - invalid_names
    write_log(len(need_sign), 'need sign in.')
    try:
        subprocess.run(
            ['write_record'],
            capture_output = False,
            encoding = 'utf-8',
            text = True,
            creationflags = subprocess.CREATE_NO_WINDOW,
            input = f"{config['dbname']} {config['passwd']} {read_today_info().index(lesson)} c"
                + '\n' + ' '.join(need_sign),
            check = True
        )
    except subprocess.CalledProcessError:
        write_log('write_record failed.')
    else:
        write_log('write_record succeeded.')

def write_log(*args, **kwargs):
    print(time.asctime(), *args, **kwargs, flush = True)

def get_watched():
    watched = [LessonInfo(s, e) for (s, e) in config['watchdog']]
    watched = [(LessonInfo.to_sec_cnt(lesson.end), lesson) for lesson in watched]
    watched.sort(key = lambda x: x[0])
    return watched

if __name__ == '__main__':
    with open('watchdog.log', 'w', encoding = 'utf-8') as logfile:
        # Do a little redirection
        sys.stdout = logfile
        write_log('watchdog version', config['version'], 'is launched.')
        # The second counts of the watched sessions
        watched = get_watched()
        write_log(' On schedule:\n', tuple(map(lambda x: x[1], watched)), sep = '')
        curr = 0
        while True:
            ct = time.localtime()
            sec_cnt = ct.tm_hour * 3600 + ct.tm_min * 60 + ct.tm_sec
            if os.path.exists('reread.txt'):
                import json
                config = json.load(open('man.json', 'r', encoding = 'utf-8'))
                os.remove('reread.txt')
                watched = get_watched()
                write_log("Schedule updated\n", tuple(map(lambda x: x[1], watched)), sep = '')
                curr = 0
                continue
            if curr < len(watched):
                if sec_cnt > watched[curr][0]:
                    # This DK has passed, move on.
                    write_log(watched[curr][1], 'has passed.')
                    curr += 1
                    continue
                elif sec_cnt > watched[curr][0] - 60:
                    # In last minute, quickly take action.
                    worker(watched[curr][1])
                    curr += 1
                    continue
            time.sleep(15)