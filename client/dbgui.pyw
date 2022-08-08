from gui_common import *
import sys
from tkinter import *

if __name__ == '__main__':
    Tk().title('Spirit GUI')
    asker = IPAsker()
    asker.mainloop()
    # if the machine data is still None, then the exit was an accident.
    # Just terminate the program.
    if asker.machine_data is None:
        sys.exit(0)
    # Now asker.machine_data contains meaningful things
    host, lessons = asker.host, asker.machine_data
    del asker
    picker = LessonPicker(lessons, host)
    picker.mainloop()
    if picker.sessid is None:
        sys.exit(0)
    # Now sessid contains the data user chose.
    sessid = picker.sessid
    del picker
    reporter = Reporter(host, sessid)
    reporter.mainloop()