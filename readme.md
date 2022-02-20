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

1. The tested compiler is MinGW-w64/GCC 8.2.0, with mingw32-make.
2. This project uses library
   [Better SQLite3 Multiple Cipher](https://github.com/utelle/SQLite3MultipleCiphers).
   Tested version is v1.3.5.

## Procedure for building and deployment

1. On the build machine, download and unzip or clone the source code into a directory.
2. Check and double check that your client and server are on the same network.
3. Make sure that your editor supports UTF-8. This is especially important for the man.json file.
4. On a terminal, `cd` into that directory.
5. Create a `include/` directory containing headers of the external library. You need to download
   a copy of the library's DLL (x64 version) into the source directory, among with the source files.
6. On the server, find the `localData.db` file and crack the password.
   (I am not getting too involved in this)
7. Run `ipconfig` and check the subnet mask and the host's IP. Now go back to the builder.
8. Run `make -j 3` in the source directory.
   If you would like optimizations, you can turn it on with `-e DEBUG=0`
9. Create two files called `man.json` and `cli.json` respectively. These are the configuration files.
    In `man.json`, put in these entries:

    * gs_path: Path to the GS executable (Forward slashes are OK)
    * port: The port in which our programs communicate. DO NOT USE PORTS LOWER THAN 1024.
    * version: An arbitary string, usually as seen in the release page.
    * dbname: Path to the database (You just found this a few steps ago)
    * passwd: The password of the database.

    In `cli.json`, put in these entries:

    * port: This must be the same as in `man.json`
    * broadcast: The default broadcasting IP address. Use your knowledge of addresses and the result
      of `ipconfig` to help you.
    * buffsize: Size of buffer when doing communications. If set too small, program will break.
      Recommends 2048
    * defmachine: The default machine to seek for.
    * timeout: Timeout before client decides that the server is down.

10. You will need to copy the following files into your removable disk:

    * `*.exe`
    * `man.json`
    * `dbman.pyw`
    * All the DLLs in the same directory as your `g++.exe` (or the compiler you use)

11. Plug in your media into the server (You will need some techniques for this.)
12. Open task manager, kill `LockMouse.exe` and restart `explorer.exe`.
13. Copy the files mentioned above into a folder.
14. Go to task scheduler, add a task that starts `dbman.pyw` 2 minutes after system boot.
15. Reboot to check.
16. When asked about the firewall, allow all access.

## How do I use this?

1. Double click on `dbgui.pyw` in an explorer window.
2. Follow the in-program instructions.
3. If something funny happens, you can launch this file with the `py` launcher to read the console
   output and try to fix it yourself. :)

*Good luck!*
