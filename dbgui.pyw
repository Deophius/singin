from tkinter import *
import dbclient, sys

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
    If not None, it is a tuple (ip, [lessons]) (see get_machine_data)
    '''
    def __init__(self, master = None):
        Frame.__init__(self, master)
        self.pack()
        self.machine_data = None
        self.make_widgets()

    def make_widgets(self):
        f1 = Frame(self)
        self.label1 = Label(f1, text = 'Machine name:', font = 'Consolas')
        self.entry = Entry(f1)
        self.entry.insert(0, config['defmachine'])
        self.entry.focus()
        self.entry.bind('<Return>', lambda event: self.callback())
        f2 = Frame(self)
        self.label2 = Label(self, text = 'No machine data yet')
        self.br = Entry(f2)
        self.br.insert(0, config['broadcast'])
        self.br.bind('<Return>', lambda e: self.callback())
        self.label1.pack(side = LEFT)
        self.entry.pack(side = LEFT)
        f1.pack(side = TOP)
        Label(f2, text = 'Broadcast ip:', font = 'Consolas').pack(side = LEFT)
        self.br.pack(side = LEFT)
        f2.pack(side = TOP)
        Button(self, text = 'OK', command = self.callback).pack(side = TOP, anchor = N)
        self.label2.pack(side = TOP)

    def callback(self):
        from tkinter.messagebox import showerror
        try:
            self.label2.config(text = 'Pending reply...')
            self.label2.update()
            self.machine_data = dbclient.get_machine_data(self.entry.get(), self.br.get())
            if self.machine_data == (None, None):
                raise ValueError("Get machine data failed! Maybe server is not up?")
            self.destroy()
            # Return to pass on knowledge about machine data
            self.quit()
            # For some reason without this return there is an error in the console.
            # It seems that after quit() the control is still in this function.
            return
        except dbclient.socket.timeout as ex:
            showerror('Read timeout', 'Get machine data failed! Maybe server is not up?')
        except ValueError as ex:
            showerror('Fail', ex.args[1])
        except dbclient.RequestFailed as ex:
            showerror('Bad server reply', ex.args[1])
        except BaseException as ex:
            showerror(str(type(ex)) + ' when getting machine data', ex.args[1])
        # No matter what failure it is, this line will be executed.
        self.label2.config(text = 'None machine data yet')
        self.label2.update()

class LessonPicker(Frame):
    ''' This window lets the user pick a lesson.
    
    It will display several buttons. When the user presses one, this component
    quits and self.sessid (initially None) is the index of the picked one
    in the list passed in.
    '''
    def __init__(self, lessons, master = None, **options):
        ''' Initializer.
        
        lessons is the list of dbclient.LessonInfo objects.
        '''
        if type(lessons) != list or type(lessons[0]) != dbclient.LessonInfo:
            raise TypeError("Incorrect type of lessons!")
        Frame.__init__(self, master, **options)
        self.pack()
        self.sessid = None
        self.__make_widgets(lessons)

    def __make_widgets(self, lessons):
        Label(self, text = "Please choose a lesson", font = 'Consolas').pack(side = TOP)
        self.__var = IntVar()
        # This is actually a bunch of radio buttons
        for num, lesson in enumerate(lessons):
            Radiobutton(
                self,
                text = str(lesson),
                value = num,
                variable = self.__var
            ).pack(side = TOP)
        self.__var.set(-1)
        Button(self, text = 'OK', command = self.__onclick).pack(side = TOP)

    def __onclick(self):
        from tkinter.messagebox import showwarning
        if self.__var.get() == -1:
            showwarning('Warning', 'You have to select a lesson!')
            return
        # Not -1, something meaningful
        self.sessid = self.__var.get()
        self.destroy()
        self.quit()

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
        print(len(self.__absent_names))
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
        except dbclient.socket.timeout as ex:
            retry_rc = askretrycancel("Retry?", 'Read timeout ' + ex.args[1])
        except ValueError as ex:
            retry_rc = askretrycancel("Retry?", 'Fail ' + ex.args[1])
        except dbclient.RequestFailed as ex:
            retry_rc = askretrycancel("Retry?", 'Bad server reply ' + ex.args[1])
        except BaseException as ex:
            retry_rc = askretrycancel(
                "Retry?",
                str(type(ex)) + ' when getting machine data ' + str(ex.args[1])
            )
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
        self.__listbox = Listbox(f, selectmode = EXTENDED)
        self.__listbox.insert(END, *self.__absent_names)
        self.__sbar = Scrollbar(f)
        self.__sbar.config(command = self.__listbox.yview)
        self.__listbox.config(yscrollcommand = self.__sbar.set)
        self.__sbar.pack(side = RIGHT, fill = Y)
        self.__listbox.pack(side = TOP, expand = True, fill = BOTH)
        f.pack(side = TOP)
        g = Frame(self)
        Button(g, text = 'OK', command = self.__confirm_and_write).pack(side = LEFT)
        Label(g, text = '        ').pack(side = LEFT)
        Button(g, text = 'Refresh', command = self.__refresh).pack(side = LEFT)
        g.pack(side = TOP)
        Button(self, text = 'Restart GS and quit', command = self.__restart).pack(anchor = N)

    def __confirm_and_write(self):
        ''' First ask the user to confirm his/her input, then sends it to server. '''
        win = Toplevel()
        win.title('Choose sign in method')
        Label(win, text = 'Which method would you like?').pack(side = TOP, expand = TRUE, fill = X)
        Button(win, text = 'Random', command = lambda: self.__write('r', win), underline = 0).pack(side = RIGHT)
        Label(win, text = '  ').pack(side = RIGHT)
        Button(win, text = 'Current', command = lambda: self.__write('c', win), underline = 0).pack(side = RIGHT)
        Label(win, text = '  ').pack(side = RIGHT)
        Button(win, text = 'Repick', command = win.destroy, underline = 2).pack(side = RIGHT)
        win.focus_set()
        win.grab_set()
        win.wait_window()

    def __write(self, mode, win):
        ''' Underlying writer. mode: c or r; win: the dialog window. '''
        from tkinter.messagebox import showerror, showinfo
        print(self.__listbox.curselection())
        try:
            dbclient.write_record(
                self.__sessid,
                self.__host,
                [self.__absent_names[i] for i in self.__listbox.curselection()],
                mode
            )
        except dbclient.RequestFailed as ex:
            showerror('Request failed', ex.args[1] + '\nPlease try again.')
        except BaseException as ex:
            showerror('Unknown error', str(type(ex)) + '\n' + str(ex.args))
        win.destroy()
        self.__getdata()
        if len(self.__absent_names) == 0:
            showinfo('Good job!', 'Everybody has signed in! I might as well go back to sleep!')
            sys.exit(0)
        self.__listbox.delete(0, END)
        self.__listbox.insert(END, *self.__absent_names)
        self.__label.config(text = 'Last call OK! Choose more:')

    def __refresh(self):
        from tkinter.messagebox import showerror, showinfo
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
            print(ex.args)
            showerror('Request failed', str(ex.args[1]) + '\nPlease try again.')
        except BaseException as ex:
            showerror('Unknown error', str(type(ex)) + '\n' + str(ex.args))
        else:
            self.destroy()
            self.quit()

if __name__ == '__main__':
    Tk().title('我找不到钟小平')
    asker = IPAsker()
    asker.mainloop()
    # if the machine data is still None, then the exit was an accident.
    # Just terminate the program.
    if asker.machine_data is None:
        sys.exit(0)
    # Now asker.machine_data contains meaningful things
    host, lessons = asker.machine_data
    del asker
    picker = LessonPicker(lessons)
    picker.mainloop()
    if picker.sessid is None:
        from sys import exit
        exit(0)
    if picker.sessid is None:
        sys.exit(0)
    # Now sessid contains the data user chose.
    print(picker.sessid)
    sessid = picker.sessid
    del picker
    reporter = Reporter(host, sessid)
    reporter.mainloop()