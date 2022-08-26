# Welcome to singin

We aim to automate the card sign in process for HEZ, and more!

## What this project is about

This project is highly specific. It only solves one problem.
I'm sure you know what I am talking about if you already know about our situation.

## Why is this project's name spelled wrong?

That's because it was wrong when the target program was developed.

## Requirements on the client side

1. Python 3.8 or 3.10, it might work on other versions, but it's not tested.
2. Python's Tkinter support (some installations don't come with one).

## Requirements on the server side

1. Windows 7 (x64) (Unsupported on other platforms, untested on other versions).
   But anyway, why would you care about the server side's OS? There's no choice.
2. The target program might update, breaking this program. But I've already warned
   you, this project is highly specific.

## Required tools and libraries for building

1. The tested compiler is MinGW-w64/GCC 8.2.0, with mingw32-make.
2. CMake version 3.23.2
3. This project uses library
   [Better SQLite3 Multiple Cipher](https://github.com/utelle/SQLite3MultipleCiphers).
   Tested version is v1.4.8.
4. [Nlohmann JSON](https://github.com/nlohmann/json), version 3.10.5
5. The icon for the executable was provided by [Aha soft](http://www.aha-soft.com/).
6. [Boost C++ libraries](https://boost.org), version 1.79.0.

Thanks for all the tools, libraries and resources the community provides so generously!

## Procedure for building and deploying our project

1. On the build machine, download and unzip or clone the source code into a directory.
2. Check and double check that your client can access the server.
3. Make sure that your editor supports UTF-8.
4. On a terminal, `cd` into that directory.
5. Create a `include/` directory containing headers of the external library. Place SQL headers
   in `include/` and JSON headers in `include/nlohmann`. Kindly put `sqlite3mc_x64.dll` in
   the root directory of the project.
6. Configure and compile using cmake. If your linker complains about spirit.rc.res is not an object,
   try explicitly telling cmake that you're using `windres` as the RC compiler. Also check boost path.
   Because Boost.asio is quite large a library, we recommend precompiling it to accelerate the build.
   However, this isn't strictly necessary.
7. Create config files as shown below.
8. You will need to copy the following files into your removable disk:

    * `cppser/spiritd.exe`
    * `man.json`, the server config file.
    * All the DLLs in the same directory as your `g++.exe` (or the compiler you use)
    * `sqlite3mc_x64.dll` and `cppsper/libspirit.dll`

9. Plug in your media into the server (You will need some techniques for this.)
10. Open task manager, kill `LockMouse.exe` and restart `explorer.exe`.
11. Copy the files mentioned above into a folder.
12. Go to task scheduler, add a task that starts `spiritd.exe` 2 minutes after system boot.
13. Reboot to check.
14. When asked about the firewall, allow all access.

## Server configuration file

The file is named `man.json`. A template looks like this:

```json
{
    "serv_port": 8303,
    "gs_port": 10031,
    "dbname": "D:/singin/localData.db",
    "passwd": "123",
    "url_stu_new": "http://127.0.0.1//Services/SmartBoard/SmartBoardLoadSingInStudentNew/json",
    "intro": "Spirit version 3.1 is up!",
    "watchdog_poll": 15,
    "retry_wait": 3,
    "keep_logs": 3
}
```

* serv_port: The port that we should use.
* gs_port: The port of the GS executable
* dbname: The path to the database.
* passwd: The password required to access the database.
* url_stu_new: The URL to post for student info.
* intro: The first line to appear in singer.log, customizable.
* watchdog_poll: The seconds watchdog waits before checking for lessons, quit, etc.
* retry_wait: If the previous attempt to auto sign in failed because of network error, wait for
  `retry_wait` seconds before retrying.
* keep_logs: The number of logs to keep before rotating over to an older one.

## Client configuration file

This file is called `cli.json`, with a template given below:

```json
{
    "port": 8303,
    "defhost": "127.0.0.1",
    "buffsize": 2048,
    "defmachine": "BJ303",
    "timeout": 10,
    "yearbook": {
      "sp": "spirit",
      "dp": "deophius"
    }
}
```

* port: The port that we use to communicate with the server.
* defhost: The default host to inquire for information in the GUI.
* buffsize: The size of the buffer into which the response will be received.
  This shouldn't be too small (<1024) and would preferentially be a power of 2.
* defmachine: The default machine ID in the GUI.
* timeout: Timeout before the GUI decides that the server is not responding.
* yearbook: An *optional* object holding the abbreviation mapping used by the quick find box.
  For example, if the config looks exactly like the one shown above, you can type `sp` in the
  quick find box and press enter. If the name `spirit` is in the list of absent people, it will
  automatically be selected. *This object is optional!* If not present, the quick find function
  will be disabled.

## Operating the GUI with keyboard only

### First page (machine info)

Press `enter` to submit the machine name and host IP info.
If you want to use a non-default machine IP or machine name, you can press
`tab` to switch to different input boxes.

### Second page (LessonPicker)

* For the first 9 lessons that appear on the list, you can press `1` to `9` to
choose them. `1` is for the one on the top of the list.
* Press `enter` should have the same effect as pressing the OK button.
* Press `p` to trigger "pause watchdog".
* Press `r` to trigger "resume watchdog".
* Press `g` to trigger "get latest news".

### Third page (Reporter)

* To quickly search through the list of absent people, use the quickfind input box.
  The box grabs the focus on default, so this is completely keyboard friendly.

  You need to specify the abbreviation mapping in the config file. See the `cli.json`
  section for details.

* Different from the previous page, if you want to trigger the buttons with your keyboard,
  you need to use a alt key combination. See the in-program hints.

### Special: using colors

We currently use green, orange and red to indicate the status of our program.

* Green means that the last operation was successful.
* Orange means that a lengthy operation, usually a request to the server, is being performed.
* Red means that the last operation resulted in an error.

## Our singin protocol

This is a simple, stateless, UDP-based protocol that uses JSON as the "mime type".
The client sends a request to the server. The request must include a `command` param, with
additional data.
The server's response always contains a `success` param, set to `true` if operation succeeds and
`false` otherwise. If `success` is `false`, then there will always be a `what` param providing
a brief explanation of the error.
The names of the commands should be self-explaining.
Here we make a listing of the implemented commands and their syntax:

### report_absent

```json
{"command": "report_absent", "sessid": 1}
   -> {"success": true, "name": ["xxx", "xxx", ...]} on success
   -> {"success": false, "what": "error description"} on failure
```

`sessid` is a non-negative integer. The order of lessons is assigned according to the sequence they
appear in the database. On the client side, this can be determined from the response of `today_info`.
The server implements range checks on `sessid`.

### write_record

```json
{"command": "write_record", "name": ["xxx", "yyy"], "sessid":1}
  -> {"success": true}
  -> {"success": false, "what": "error description"}
```

`name` is a list of Chinese names. `sessid` is same as above.
The server catches all SQL errors and returns them as `what` if database operations fail.
Because we use JSON as the "mime type", the name strings should be UTF-8 encoded.

### restart_gs

``` json
{"command": "restart_gs"}
  -> {"success": true}
  -> {"success": false, "what": "error description"}
```

Restarts GS. On the current implementation, this will always succeed.

### today_info

```json
{"command":"today_info", "machine": "NJ303"}
   -> {"success": true, "end": ["07:20:00", "08:30:00"]}
   -> {"success": false, "what":"Wrong machine", "machine":"BJ303"}
   -> {"success": false, "what": "error description"}
```

Gets the lessons there are today. *Different from versions 2.x*, broadcast is no longer used!
The client must know the IP of the server *and* its machine ID to access the server.
If the `machine` in the request matches that of the DB, returns the list of lessons represented
as their "endtimes" in the format shown above.
If these two don't match, returns the machine ID in the database along with an error message.

### quit_spirit

```json
{"command": "quit_spirit"}
   -> {"success": true}
```

Exits the server. Normally, this will always return true, as reporting errors in the destructors
is kind of difficult.

### flush_notice

```json
{"command": "flush_notice"}
   -> {"success": true}
```

Using GS's port, we can inform it to get the latest notice immediately. This might be useful if
too much network analysis breaks the network interface configuration and you cannot receive
notifications at all. Because of the implementation, we cannot tell whether the command really took
effect. We can only guarantee that a datagram will be sent.

### doggie_stick

```json
{"command": "doggie_stick", "pause": true or false}
   -> {"success": true}
   -> {"success": false, "what": "..."} on errors, like a missing pause argument.
```

Tells the watchdog to pause or resume, respectively. Usually atomic bool operations are noexcept,
so we will simply return a success.

*Good luck!*
