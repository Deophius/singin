# Welcome to singin

We aim to automate the card sign in process for HEZ, and more!

## What this project is about

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
2. The target program might update, breaking this program. But I've already warned
   you, this project is highly specific.

## Required tools and libraries for building

1. The tested compiler is MinGW-w64/GCC 8.2.0, with mingw32-make.
2. CMake version 3.23.2
3. This project uses library
   [Better SQLite3 Multiple Cipher](https://github.com/utelle/SQLite3MultipleCiphers).
   Tested version is v1.4.4.
4. [Nlohmann JSON](https://github.com/nlohmann/json), version 3.10.5

## Procedure for building and deployment of our project

1. On the build machine, download and unzip or clone the source code into a directory.
2. Check and double check that your client can access the server.
3. Make sure that your editor supports UTF-8.
4. On a terminal, `cd` into that directory.
5. Create a `include/` directory containing headers of the external library. Place SQL headers
   in `include/` and JSON headers in `include/nlohmann`.
6. Configure and compile using cmake.
7. Create config files as shown below.
8. You will need to copy the following files into your removable disk:

    * `cppser/watchd.exe`
    * `man.json`, the server config file.
    * All the DLLs in the same directory as your `g++.exe` (or the compiler you use)
    * `sqlite3mc_x64.dll`

9. Plug in your media into the server (You will need some techniques for this.)
10. Open task manager, kill `LockMouse.exe` and restart `explorer.exe`.
11. Copy the files mentioned above into a folder.
12. Go to task scheduler, add a task that starts `dbman.pyw` 2 minutes after system boot.
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
    "url_stu_new": "http://192.168.16.85//Services/SmartBoard/SmartBoardLoadSingInStudentNew/json",
    "intro": "Spirit version 3.0 is up!"
}
```

* serv_port: The port that we should use.
* gs_port: The port of the GS executable
* dbname: The path to the database.
* passwd: The password required to access the database.
* url_stu_new: The URL to post for student info.
* intro: The first line to appear in singer.log, customizable.

## Our singin protocol

This is a simple, stateless, UDP-based protocol that uses JSON as the "mime type".
The client sends a request to the server. The request must include a `command` param, with
additional data.
The server's response always contains a `success` param, set to `true` if operation succeeds and
`false` otherwise. If `success` is `false`, then there will always be a `what` data member providing
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
The server catches all SQL errors and return them as `what` if database operations fail.

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
as their "endtimes". The time strings have the format shown above.
If these two don't match, returns the machine ID in the database along with an error message.

*Good luck!*
