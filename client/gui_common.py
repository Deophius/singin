# Home-brew library of the GUI classes used.
# Separating them from dbgui.pyw is because a py extension allows imports.
from tkinter import *
import dbclient, sys

__all__ = 'IPAsker LessonPicker Reporter'.split()

try:
    import json
    config = json.load(open('cli.json', encoding = 'utf-8'))
except FileNotFoundError:
    from tkinter.messagebox import showerror
    showerror('No config', 'Please add configuration file cli.json')
    sys.exit(1)

class IPAsker(Frame):
    ''' A component that executes get_machine_data()
    
    Result is stored in self.machine_data. If that is None, then we have no data.
    If not None, it is a list (see get_machine_data)

    The IP address the user input is in self.host
    '''
    def __init__(self, master = None):
        Frame.__init__(self, master)
        self.pack()
        self.machine_data = None
        self.host = None
        self.make_widgets()

    def make_widgets(self):
        f1 = Frame(self)
        self.label1 = Label(f1, text = 'Machine name:', font = 'Consolas')
        self.entry = Entry(f1, font = 'Consolas')
        self.entry.insert(0, config['defmachine'])
        self.entry.focus()
        self.entry.bind('<Return>', lambda event: self.callback())
        f2 = Frame(self)
        self.label2 = Label(self, text = 'No machine data yet', font = 'Consolas')
        self.br = Entry(f2, font = 'Consolas')
        self.br.insert(0, config['defhost'])
        self.br.bind('<Return>', lambda e: self.callback())
        self.label1.pack(side = LEFT)
        self.entry.pack(side = LEFT)
        f1.pack(side = TOP)
        Label(f2, text = 'Host ip:', font = 'Consolas').pack(side = LEFT)
        self.br.pack(side = LEFT)
        f2.pack(side = TOP)
        Button(self, text = 'OK', command = self.callback, font = 'Consolas').pack(side = TOP, anchor = N)
        self.label2.pack(side = TOP)

    def callback(self):
        from tkinter.messagebox import showerror
        try:
            self.label2.config(text = 'Pending reply...')
            self.label2.update()
            self.host = self.br.get()
            self.machine_data = dbclient.get_machine_data(self.entry.get(), self.host)
            self.destroy()
            # Return to pass on knowledge about machine data
            self.quit()
            # For some reason without this return there is an error in the console.
            # It seems that after quit() the control is still in this function.
            return
        except dbclient.RequestFailed as ex:
            showerror('Request failed', ex.args[1])
        # No matter what failure it is, this line will be executed.
        self.label2.config(text = 'None machine data yet')
        self.label2.update()

class LessonPicker(Frame):
    ''' This window lets the user pick a lesson.
    
    It will display several buttons. When the user presses one, this component
    quits and self.sessid (initially None) is the index of the picked one
    in the list passed in.
    '''
    def __init__(self, lessons, host, master = None, **options):
        ''' Initializer.
        
        lessons is the list of dbclient.LessonInfo objects.
        '''
        Frame.__init__(self, master, **options)
        self.pack()
        self.sessid = None
        self.__host = host
        self.__make_widgets(lessons)

    def __make_widgets(self, lessons):
        Label(self, text = "Please choose a lesson", font = 'Consolas').pack(side = TOP)
        self.__var = IntVar()
        # This is actually a bunch of radio buttons
        for num, lesson in enumerate(lessons):
            r = Radiobutton(
                self,
                text = str(lesson),
                value = num,
                variable = self.__var,
                font = 'Consolas'
            )
            if num < 10:
                r.bind_all(str(num + 1), lambda event, num = num: self.__var.set(num))
            r.pack()
        self.__var.set(-1)
        ok_button = Button(self, text = 'OK', command = self.__onlesson, font = "Consolas")
        ok_button.bind_all('<Return>', lambda event: self.__onlesson())
        ok_button.pack(side = TOP)
        Label(self, text = "\nMisc operations:", font = "Consolas").pack(side = TOP)
        pause_button = Button(self, text = "Pause watchdog", command = self.__on_pause_dog, font = "Consolas")
        pause_button.bind_all('p', lambda event: self.__on_pause_dog())
        pause_button.pack()
        resume_button = Button(self, text = "Resume watchdog", command = self.__on_resume_dog, font = "Consolas")
        resume_button.bind_all('r', lambda event: self.__on_resume_dog())
        resume_button.pack()
        news_button = Button(self, text = "Get news", command = self.__on_flush_notice, font = "Consolas")
        news_button.bind_all('g', lambda event: self.__on_flush_notice())
        news_button.pack()

    def __onlesson(self):
        from tkinter.messagebox import showwarning
        if self.__var.get() == -1:
            showwarning('Warning', 'You have to select a lesson!')
            return
        # Not -1, something meaningful
        self.sessid = self.__var.get()
        for i in 'p r g <Return>'.split():
            self.unbind_all(i)
        self.destroy()
        self.quit()

    def __on_pause_dog(self):
        dbclient.doggie_stick(self.__host, True)

    def __on_resume_dog(self):
        dbclient.doggie_stick(self.__host, False)

    def __on_flush_notice(self):
        dbclient.flush_notice(self.__host)

class Reporter(Frame):
    ''' Report absent and ask who will sign in. Return value in self.signin_names,
    initially None.
    '''
    def __init__(self, host, sessid, master = None, **options):
        ''' host: An IP. sessid: the id of the DK session. options: passed on '''
        Frame.__init__(self, master, **options)
        self.pack()
        self.signin_names = None
        self.__absent_names = None
        self.__host, self.__sessid = host, sessid
        # This label is at the top of the page
        self.__label = Label(self, font = "Consolas")
        self.__label.pack()
        self.__getdata()
        # If reached here, user had retried until there is a good response
        assert type(self.__absent_names) == list
        self.__showdata()

    def __getdata(self):
        ''' Gets data. Stores list of absent people in self.__absent_names '''
        from tkinter.messagebox import askretrycancel, showerror
        self.__label.config(text = 'Please wait a sec...')
        self.update()
        # User reply for retry, defaults to False, so when nothing special occurs,
        # doesn't retry by default
        retry_rc = False
        try:
            self.__absent_names = dbclient.report_absent(self.__sessid, self.__host)
        except dbclient.RequestFailed as ex:
            retry_rc = askretrycancel("Retry?", 'Request failed: ' + ex.args[1])
        else:
            # No exception occurred, just return
            return
        if retry_rc:
            self.__getdata()
            return
        else:
            showerror("I'm hungry on information", "Can't proceed with no server connection.")
            sys.exit(0)

    def __showdata(self):
        from tkinter.messagebox import showinfo
        if len(self.__absent_names) == 0:
            showinfo('Good job!', 'Everybody has signed in! I might as well go back to sleep!')
            sys.exit(0)
        self.__label.config(text = 'These people didn\'t DK:')
        f = Frame(self)
        self.__listbox = Listbox(f, selectmode = EXTENDED, font = ('YaHei', 20))
        self.__listbox.insert(END, *self.__absent_names)
        self.__sbar = Scrollbar(f)
        self.__sbar.config(command = self.__listbox.yview)
        self.__listbox.config(yscrollcommand = self.__sbar.set)
        self.__sbar.pack(side = RIGHT, fill = Y)
        self.__listbox.pack(side = TOP, expand = True, fill = BOTH)
        if 'yearbook' in config:
            Label(f, text = "Quick find:", font = 'Consolas').pack(side = LEFT)
            self.__quickfind = Entry(f, font = 'Consolas')
            self.__quickfind.bind('<Return>', lambda e: self.__find_abbrev())
            self.__quickfind.focus()
            self.__quickfind.pack(side = LEFT)
        f.pack()
        # Open a new frame for better layout
        g = Frame(self)
        ok_button = Button(g, text = 'Write (Alt-W)', command = self.__write, font = 'Consolas')
        ok_button.bind_all('<Alt-w>', lambda event: self.__write())
        ok_button.pack()
        refresh_button = Button(g, text = 'Refresh (Alt-R)', command = self.__refresh, font = 'Consolas')
        refresh_button.bind_all('<Alt-r>', lambda event: self.__refresh())
        refresh_button.pack()
        restart_button = Button(g, text = 'Restart GS & quit (Alt-Q)', command = self.__restart, font = "Consolas")
        restart_button.bind_all('<Alt-q>', lambda e: self.__restart())
        restart_button.pack()
        g.pack()

    def __write(self):
        ''' Underlying writer.'''
        from tkinter.messagebox import showerror, showinfo
        try:
            dbclient.write_record(
                self.__sessid,
                self.__host,
                [self.__absent_names[i] for i in self.__listbox.curselection()]
            )
        except dbclient.RequestFailed as ex:
            showerror('Request failed', ex.args[1] + '\nPlease try again.')
        except BaseException as ex:
            showerror('Unknown error', str(type(ex)) + '\n' + str(ex.args))
        self.__getdata()
        if len(self.__absent_names) == 0:
            showinfo('Good job!', 'Everybody has signed in! I might as well go back to sleep!')
            sys.exit(0)
        self.__listbox.delete(0, END)
        self.__listbox.insert(END, *self.__absent_names)
        self.__label.config(text = 'Last call OK! Choose more:')

    def __refresh(self):
        from tkinter.messagebox import showinfo
        self.__getdata()
        if len(self.__absent_names) == 0:
            showinfo('Good job!', 'Everybody has signed in! I might as well go back to sleep!')
            sys.exit(0)
        self.__listbox.delete(0, END)
        self.__listbox.insert(END, *self.__absent_names)
        self.__label.config(text = 'Refreshed! Choose more:')

    def __restart(self):
        ''' Does the communication and restarts GS terminal. '''
        from tkinter.messagebox import showerror, askyesnocancel
        if not askyesnocancel('Restart?', 'Really restart terminal?'):
            return
        try:
            dbclient.restart_gs(self.__host)
        except dbclient.RequestFailed as ex:
            showerror('Request failed', str(ex.args[1]) + '\nPlease try again.')
        except BaseException as ex:
            showerror('Unknown error', str(type(ex)) + '\n' + str(ex.args))
        else:
            self.destroy()
            self.quit()

    def __find_abbrev(self):
        ''' Finds the initials in self.__quick_find and selects it.
        If not found, sets the top label to an error msg.
        '''
        abbrev = self.__quickfind.get()
        self.__quickfind.delete(0, END)
        # Defensive programming.
        assert type(self.__absent_names) == list
        # We have already checked in showdata() that the config is there.
        # Just assume that it was written correctly.
        if abbrev not in config['yearbook']:
            self.__label.configure(text = 'Abbrev not in config!')
            return
        try:
            index = self.__absent_names.index(config['yearbook'][abbrev])
        except ValueError:
            self.__label.configure(text = 'Not in absent list')
            return
        self.__listbox.selection_set(index)
        self.__listbox.yview_moveto(index / len(self.__absent_names))
        self.__label.configure(text = f'Selected {abbrev}')
