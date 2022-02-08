# What this project is about

This project is highly specific. It only solves one problem.
I'm sure you know what I am talking about if you already know about our situation.

## Why is this project's name spelled wrong?

That's because it was wrong when the target program was developed.

## Requirements on the client side

1. Python 3.8, it might work on other versions, but it's not tested.
2. It should be on the same network as the server! (Important for broadcasting)

## Requirements on the server side

1. Windows 7 (x64) (Unsupported on other platforms, untested on other versions).
   But anyway, why would you care about the server side's OS? There's no choice.
2. Python 3.8, same as above.
3. The target program might update, breaking this program. But I've already warned
   you, this project is highly specific.

## Required tools and libraries for building

1. The tested compiler is MinGW-w64/GCC 8.1.0, with mingw32-make.
2. This project uses library [Better SQLite3 Multiple Cipher](https://github.com/utelle/SQLite3MultipleCiphers)
   Tested version is v1.3.5.

## Procedure for building and deployment

1. On the build machine, download and unzip or clone the source code into a directory.
2. Check and double check that your client and server are on the same network.
3. On a terminal, `cd` into that directory.
4. Make sure that your editor supports UTF-8. This is especially important for the C++ part.
5. Create a `include/` directory contains headers of the external library. You need to download a copy of the library
   DLL (x64 version) into the source directory, among with the source files.
6. Open `dbman.h` in an editor and find the constant variables `dbname` and `passwd`.
7. On the server, find the `localData.db` file and crack the password. (I am not getting too involved in this)
8. Back to the builder, configure these variables as seen on the server.
9. Run `ipconfig` and check the subnet mask.
10. Open `dbclient.py` and find `m` in `get_broadcast_ip()`. Configure it according to your situation.
11. Run `make -j 4` in the source directory. If you would like optimizations, you can turn it on with `-e DEBUG=0`
12. Find the target program's path and configure `gs_path` in `dbman.pyw`.
13. If you prefer, you can also configure `port` in both `dbman.pyw` and `dbclient.py`.
14. You will need to copy the following files into your removable disk:

    * `*.exe`
    * `dbman.pyw`
    * All the DLLs in the same directory as your `g++.exe`.

15. Plug in your media into the server (You will need some techniques for this.)
16. Open task manager, kill `LockMouse.exe` and restart `explorer.exe`.
17. Copy the files mentioned above into a folder.
18. Go to task scheduler, add a task that starts `dbman.pyw` 3 minutes after system boot.
19. Reboot to check.

## How do I use this?

1. On the client, open a shell and run `dbclient.py`.
2. Follow the instructions.
3. If an error occurs, I think you will be able to figure out why since you already have the
   server up and running, which proves that you are capable enough.

*Good luck!*
