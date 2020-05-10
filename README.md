# snowHTTP

## Features
* highly configurable, check out `lib/snowhttp.h`
* no mid-run memory allocations outside of wolfssl and potential cache refreshes
* multithreading
* tls session resumption (+ tickets) - _sessions are cached at startup and refreshed on a timer_
* dns caching

## Building
To build library as static:
```console
$ make
```

To build example executable:
```console
$ make
$ make example
```

All built files are created by default in `bin/`


## Usage
#### Single loop (no multithreading) setup
```c
    snow_global_t global = {}; // contains all data
    
    // optional, if TLS session reuse is wanted
    snow_addWantedSession(&global, "https://hostname.com"); 
    snow_addWantedSession(&global, "https://hostname1.com");
    
    snow_init(&global);
    ...
    ev_run(EV_DEFAULT, loop_cb);
    ...
    snow_destroy(&global);
```
Requests can be made from loop_cb:
```c
snow_do(&global, GET, "https://google.com/", http_cb, err_cb);
```
#### Multi loop setup
See `example.cpp`.
```c
    snow_global_t global = {}; // contains all data
    ev_loop loops[multi_loop_max]; // multi loop data

    // optional, if TLS session reuse is wanted
    snow_addWantedSession(&global, "https://hostname.com"); 
    snow_addWantedSession(&global, "https://hostname1.com");
    
    snow_init(&global);
    snow_spawnLoops(&global); // creates threads
    ...
    snow_joinLoops(&global); // joins threads
    ...
    snow_destroy(&global);
```
Requests can be made from the main thread, before snow_joinLoops:
```c
snow_do(&global, GET, "https://google.com/", http_cb, err_cb);
```

## License
This software is distributed under a MIT license, see `LICENSE`.  
No warranty is provided, use at own risk.