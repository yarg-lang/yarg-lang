# Yarg Startup

When Yarg starts up the first Yarg code can be supplied in main.ya. This is always executed. If this terminates, then a REPL is started. This will currently interact on the stdout/stdin offered by the microcontrollers C runtime. On a Pico that is currently USB serial and UART0.

## Hosted Yarg

When yarg is hosted by an OS, it includes support for running a Yarg script directly, skipping main.ya and not defaulting to a REPL. When it terminates, the process hosting yarg exits normally, and control is returned to the host OS.

### Hosted Yarg Library

When hosted, Yarg provides an additional library for interacting with the host OS.

```
var hostArgs;
```

This will be an array of string arguments, starting with the script name, and followed by any arguments passed to the script.

```
host_exitCode(n);
```

This will set an exit code that will be used to exit the hosting cyarg process. 0 will be used by default.

### cyarg implementation

Currently, Yarg is implemented by cyarg. By default, as used on microcontrollers, this starts from a zero parameter entry function, and in practice never terminates.

cyarg can be additionally configured (CYARG_HOSTING=HOSTED) to support hosting by an OS, intended for development of the language and testing on a host PC. In this case, cyarg can be supplied a script name and any arguments to be passed to the script. See cyarg --help for usage notes on a host OS.

cyarg implements the Yarg startup in cyarg.ya by default, and cyarg-hosted.ya when running on a host OS.
