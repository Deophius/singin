# GUI component to "train" the watchdog
from tkinter import *
import dbclient, sys, gui_common

__all__ = []

if __name__ == '__main__':
    Tk().title('搞打卡竞赛，进国家队，拿亚洲金牌！')
    asker = gui_common.IPAsker()
    asker.mainloop()
    if asker.machine_data is None:
        sys.exit(0)
    trainer = gui_common.Trainer(asker.machine_data[0])
    trainer.mainloop()