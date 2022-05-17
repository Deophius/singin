# Notnet, wrappers for networking concerning notifications.
import logging

try:
    from json import load
    config = load(open('news.json', encoding = 'utf-8'))
except FileNotFoundError:
    logging.critical('Configuration not found', 'Please create the configuration file in cwd.')

class ServerError(RuntimeError):
    def __init__(self, *args):
        RuntimeError.__init__(self, *args)

def do_post(sess, url, head, body, debugfile):
    ''' Do a typical post request

    head and body are supposed to be dictionaries.
    The result will be a dictionary, too.
    debugfile is the file to read from when in debugging mode. (str)
    '''
    import requests
    from json import dumps, load
    from json import JSONDecodeError
    try:
        timeout = config['timeout']
    except KeyError:
        logging.info('No timeout specified, default to 10s')
        timeout = 10
    debug = ('debug' in config) and config['debug']
    if debug:
        with open(debugfile, encoding = 'utf-8') as fp:
            return load(fp)
    # Not in debug mode
    prep = requests.Request('POST', url = url, headers = head, data = dumps(body)).prepare()
    try:
        req = sess.send(prep, timeout = timeout).json()
        return req
    except JSONDecodeError as ex:
        logging.warning('Reply is not valid JSON, ' + ex.args[0])
    except requests.RequestException as ex:
        logging.warning('Network error, ' + str(type(ex.args[0])))
        logging.info('When trying to access ' + ex.args[0].args[0])
    return {}

def download_tasks(sess):
    '''' Posts download tasks. Returns the list of tasks in its original form. 
    Because this is deserialized from a JSON, it will be a dictionary.
    '''
    try:
        ua = config['user_agent']
        url = config['url_down']
    except KeyError as ex:
        logging.error(f'{ex.args[0]} is not in configuration!')
        return {}
    head = {
        'Content-Type': "application/json",
        'Expect': '100-continue',
        'User-Agent': ua
    }
    body = { 'tCode': 'NJ303' }
    return do_post(sess, url, head, body, 'downtask.json')

def set_reviced(sess, pubid):
    '''  Calls the set_task_reviced API to notify the server that we've read this one.

    sess: the session to use; pubid: The task's publish ID, found in download_tasks()
    Returns True if successful, False otherwise.
    '''
    try:
        url = config['url_revice']
        ua = config['user_agent']
    except KeyError as ex:
        logging.error(f"{ex.args[0]} not in config!")
        return False
    head = {
        'Content-Type': "application/json",
        'Expect': '100-continue',
        'User-Agent': ua
    }
    body = {
        'publishid': pubid,
        'queueid': '00000000-0000-0000-0000-000000000000'
    }
    reply = do_post(sess, url, head, body, 'revice.json')
    return ('result' in reply) and reply['result']

def get_plan(sess, taskid):
    ''' Gets the task plan from API.

    sess is the session to use; taskid is got from download_tasks().
    Returns a tuple of (starttime, endtime) on success, (None, None) on failure.
    The time string has the following format: 2022-05-15T14:00:00
    Removal of this "T" is done in C++ part.
    '''
    try:
        url = config['url_plan']
        ua = config['user_agent']
    except KeyError as ex:
        logging.error("Cannot find {} in configuration".format(ex.args[0]))
        return (None, None)
    head = {
        'Content-Type': "application/json",
        'Expect': '100-continue',
        'User-Agent': ua
    }
    body = {
        'taskid': taskid
    }
    reply = do_post(sess, url, head, body, 'taskplan.json')
    if 'result' not in reply:
        return (None, None)
    return (reply['result'][0]['StartTime'], reply['result'][-1]['EndTime'])
