# Notop, common operations for notifications
from notnet import *
import logging, requests, time, subprocess

# Config is loaded from * import

class News:
	def __init__(self, f, g):
		''' Initialize self. f is an object in the result array of download_task.
		g is the tuple got from the result of get_plan
		 '''
		try:
			self.taskid = f['taskid']
			self.pubid = f['publishid']
			self.type = f['executetype']
			self.target = f['target']
			self.level = f['level']
			self.start = g[0]
			self.end = g[1]
		except KeyError as ex:
			logging.error(f'When initializing News object, {ex.args[0]} not found.')
		# Refuse full screen media
		if self.type == 'PLAYMEDIA.IMAGE':
			self.type = 'POSTER'
			self.level = 100

	def download_pic(self, name):
		''' Download one picture and put it to correct place.
		Name should contain slashes.
		Returns false on failure.
		'''
		name = name.replace('\\', '/')
		# First, get it. Because we have reached this stage, we can assume a normal network
		try:
			r = requests.get(f"http://{config['server']}//{name}")
		except requests.RequestException as ex:
			logging.warning(f'When getting {name}, encountered {str(type(ex.args[0]))}')
			return False
		filename = config['img_path'] + '/..' + name.replace('/', '.')
		logging.info('File name is ' + filename)
		with open(filename, 'wb') as fp:
			fp.write(r.content)
		return True

	def download(self):
		if self.type == 'SHADOWLANTERN':
			return
		if self.type == 'POSTER':
			piclist = self.target.split(',')
			curr = 0
			while curr < len(piclist):
				if not self.download_pic(piclist[curr]):
					time.sleep(config['timeout'])
					continue
				curr += 1

	def revice(self, sess):
		set_reviced(sess, self.pubid)

def gen_adder_input(newses):
	''' Generates the input for addnews, one line a time. newses is a list of news. '''
	yield config['dbname']
	yield config['passwd']
	for news in newses:
		yield f"{news.type} {news.start} {news.end} {news.target}"

def call_adder(newses):
	''' Returns the list of newses that need retrying. '''
	p = subprocess.run(
		['addnews'],
		capture_output = True,
		text = True,
		input = '\n'.join(gen_adder_input(newses)),
		creationflags = subprocess.CREATE_NO_WINDOW,
		encoding = 'utf-8'
	)
	if p.returncode == 0 and p.stdout.count('1') == len(newses):
		return []
	else:
		return newses[p.stdout.count('1'):]

def restart_gs():
	import os
	subprocess.run(
    	'taskkill /im GS.Terminal.SmartBoard.exe /f /t'.split(),
        creationflags = subprocess.CREATE_NO_WINDOW,
        stdout = subprocess.DEVNULL,
        stderr = subprocess.DEVNULL
    )
	os.startfile(config['gs_path'])

def do_work(sess):
	down = download_tasks(sess)
	if down == {}:
		return
	# Actually, here we can assume that the network is good
	newses = []
	for task in down['result']:
		plan = get_plan(sess, task['taskid'])
		if plan == (None, None):
			return
		newses.append(News(task, plan))
		newses[-1].download()
		newses[-1].revice(sess)
	logging.info('There are {} newses'.format(len(newses)))
	if len(newses) > 0:
		call_adder(newses)
		restart_gs()