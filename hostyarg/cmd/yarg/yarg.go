package main

import (
	"flag"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"

	"github.com/yarg-lang/yarg-lang/hostyarg"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/devicerunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceutil"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/hostrunner"
	"mellium.im/sysexit"
)

func exitWithUsageError(usagenote string) {
	fmt.Fprintln(os.Stderr, usagenote)
	fmt.Fprintln(os.Stderr, "Use 'yarg help' for more information.")
	os.Exit(int(sysexit.ErrUsage))
}

func exitWithError(errnote string) {
	fmt.Fprintln(os.Stderr, errnote)
	os.Exit(1)
}

func DefaultYargRunner(suppliedPort, suppliedInterpreter, suppliedLib string) (runner hostyarg.YargRunner, err error) {

	if suppliedPort == "" && suppliedInterpreter == "" {
		port, ok := deviceutil.DefaultPort()
		if !ok {
			return nil, fmt.Errorf("failed to find default device port")
		}
		runner = &devicerunner.DeviceRunner{Port: port}
		return runner, nil
	} else if suppliedPort != "" && suppliedInterpreter != "" {
		return nil, fmt.Errorf("cannot specify both device port and local interpreter")
	} else if suppliedInterpreter != "" {
		runner = &hostrunner.HostRunner{Compiler: hostrunner.Compiler{Interpreter: suppliedInterpreter}, LibPath: suppliedLib}
		return runner, nil
	} else if suppliedPort != "" {
		port, ok := deviceutil.PicoPortFor(suppliedPort)
		if !ok {
			return nil, fmt.Errorf("failed to find specified device port")
		}
		runner = &devicerunner.DeviceRunner{Port: port}
		return runner, nil
	}
	return nil, fmt.Errorf("invalid configuration")
}

func dispatchSubCommand(args []string) {

	if len(args) < 1 {
		exitWithUsageError("expected command")
	}

	flags := flag.NewFlagSet(args[0], flag.ExitOnError)
	switch args[0] {
	case "help":
		fmt.Println("yarg is a tool for working with Yarg sources, device images and devices running Yarg.")
		fmt.Println("Usage: yarg [-v] <command> [options]")
		fmt.Println("Commands:")
		fmt.Println("  help - show this help message")
		fmt.Println("  runtests - run tests on the host")
		fmt.Println("  format - format a device image with a littlefs filesystem")
		fmt.Println("  ls - list contents of a directory in a littlefs filesystem")
		fmt.Println("  fsinfo - print information about a littlefs filesystem")
		fmt.Println("  cp - copy a file to or from a littlefs filesystem")
		fmt.Println("  mkdir - create a directory in a littlefs filesystem")
		fmt.Println("  devices - list connected devices")
		fmt.Println("  resetdevice - reset the connected device")
		fmt.Println("  flashdevice - flash a binary to the connected device")
		fmt.Println("  compile - compile a Yarg source file, reporting errors if any")
		fmt.Println("  run - run a Yarg source file on the connected device")
		fmt.Println("  repl - start a REPL connected to the device")
		fmt.Println("device commands can specify:")
		fmt.Println("  a connected device (which is the default if found) with: ")
		fmt.Println("     --port serialportname")
		fmt.Println("  or a host based yarg system with:")
		fmt.Println("     --interpreter path/to/cyarg --lib path/to/libraries")
		os.Exit(0)
	case "format":
		formatFSFS := flags.String("image", "", "image containing filesystem to format")
		formatCreate := flags.Bool("create", false, "create image if it doesn't exist")
		flags.Parse(args[1:])
		err := deviceimage.Cmdformat(*formatFSFS, *formatCreate)
		if err != nil {
			if os.IsNotExist(err) {
				if p, ok := err.(*fs.PathError); ok {
					exitWithUsageError(fmt.Sprintf("image file does not exist: %s (use -create?)", p.Path))
				}
			} else {
				exitWithUsageError("format failed")
			}
		}
	case "ls":
		lsDirFS := flags.String("image", "", "image containing filesystem to mount")
		lsDirEntry := flags.String("dir", ".", "directory to ls")
		lsLong := flags.Bool("l", false, "long listing")
		flags.Parse(args[1:])
		if *lsDirFS == "" {
			exitWithUsageError("expect image containing filesystem to list directory from")
		}
		err := deviceimage.CmdLs(*lsDirFS, *lsDirEntry, *lsLong)
		if err != nil {
			exitWithError(err.Error())
		}
	case "fsinfo":
		lsDirFS := flags.String("image", "", "image containing filesystem to mount")
		flags.Parse(args[1:])
		if *lsDirFS == "" {
			exitWithUsageError("expect image containing filesystem to list directory from")
		}
		err := deviceimage.CmdFsInfo(*lsDirFS)
		if err != nil {
			exitWithError(err.Error())
		}
	case "cp":
		cpDestFS := flags.String("fs", "", "filesystem to mount")
		cpSrc := flags.String("src", "", "source file to copy")
		cpDest := flags.String("dest", "", "destination file to copy to")
		flags.Parse(args[1:])

		if *cpSrc == "" || *cpDest == "" {
			exitWithUsageError("expect source and destination for cp")
		}
		err := deviceimage.CmdCp(*cpDestFS, *cpSrc, *cpDest)
		if err != nil {
			exitWithError(err.Error())
		}
	case "mkdir":
		mkdirFS := flags.String("fs", "", "filesystem to mount")
		mkdirDir := flags.String("dir", "", "directory to create")
		flags.Parse(args[1:])

		if *mkdirDir == "" {
			exitWithUsageError("expect directory to create")
		}
		err := deviceimage.CmdMkdir(*mkdirFS, *mkdirDir)
		if err != nil {
			exitWithError(err.Error())
		}
	case "resetdevice":
		flags.Parse(args[1:])

		result := devicerunner.CmdResetDevice()
		if !result {
			exitWithError("reset failed")
		}
	case "flashdevice":
		deviceFlashBinary := flags.String("binary", "", "binary to flash")
		flags.Parse(args[1:])

		if *deviceFlashBinary == "" {
			exitWithUsageError("expect binary to flash")
		}

		result := devicerunner.CmdFlashBinary(*deviceFlashBinary)
		if !result {
			exitWithError("flash failed")
		}
	case "compile":
		compileSource := flags.String("source", "", "source to compile")
		compileInterpreter := flags.String("interpreter", "cyarg", "default interpreter")
		flags.Parse(args[1:])

		if *compileSource == "" {
			exitWithUsageError("expect source to compile")
		}

		runner := &hostrunner.Compiler{Interpreter: *compileInterpreter}

		err := runner.CmdCompile(*compileSource)
		if err != nil {
			exitWithError("compile failed")
		}
	case "run":
		devicePort := flags.String("port", "", "serial port to use as device console")
		localRunInterpreter := flags.String("interpreter", "", "default interpreter")
		localLib := flags.String("lib", "", "library to include")
		flags.Parse(args[1:])
		positionals := flags.Args()
		if len(positionals) != 1 {
			exitWithUsageError("unexpected positional arguments: " + fmt.Sprint(positionals))
		}
		if positionals[0] == "" {
			exitWithUsageError("expect source to run")
		}

		runner, err := DefaultYargRunner(*devicePort, *localRunInterpreter, *localLib)
		if err != nil {
			exitWithError(err.Error())
		}

		err = runner.RunInteractively(positionals[0])
		if err != nil {
			exitWithError(err.Error())
		}
	case "repl":
		devicePort := flags.String("port", "", "serial port to use as device console")
		localRunInterpreter := flags.String("interpreter", "", "default interpreter")
		localLib := flags.String("lib", "", "library to include")
		flags.Parse(args[1:])

		runner, err := DefaultYargRunner(*devicePort, *localRunInterpreter, *localLib)
		if err != nil {
			exitWithError(err.Error())
		}

		err = runner.REPL()
		if err != nil {
			exitWithError(err.Error())
		}
	case "runtests":
		testRunInterpreter := flags.String("interpreter", "cyarg", "default interpreter")
		testRunLib := flags.String("lib", "", "library to include in test runs")
		devicePort := flags.String("port", "", "serial port to use as device console")

		testHostSource := flags.String("tests", "", "host source of test to run")
		testDeviceSource := flags.String("device-source", "", "device source of test to run")

		flags.Parse(args[1:])

		runner, err := DefaultYargRunner(*devicePort, *testRunInterpreter, *testRunLib)
		if err != nil {
			exitWithError(err.Error())
		}

		if *testHostSource == "" {
			exitWithUsageError("expect host source for expect-test")
		}

		failedtestcount := 0
		if r, ok := runner.(*hostrunner.HostRunner); ok {
			if *testDeviceSource != "" {
				exitWithUsageError("device source specified but runner is host based")
			}
			err, failedtestcount = r.CmdExpectTest(*testHostSource)
		} else {
			err = fmt.Errorf("unknown runner type")
		}

		if err != nil {
			exitWithError(err.Error())
		}
		if failedtestcount > 0 {
			os.Exit(1)
		}
	case "devices":
		deviceInterpreter := flags.String("interpreter", "", "default interpreter")
		deviceLib := flags.String("lib", "", "library to include in test runs")
		flags.Parse(args[1:])
		hostyarg.CmdListDevices(*deviceInterpreter, *deviceLib)
	default:
		exitWithUsageError("unknown command")
	}
}

func dispatchGlobalFlags(args []string) (remainder []string) {
	flags := flag.NewFlagSet(args[0], flag.ExitOnError)
	verbose := flags.Bool("v", false, "enable verbose logging")
	flags.Parse(args[1:])
	remainder = flags.Args()

	if *verbose {
		log.SetOutput(os.Stdout)
	} else {
		log.SetOutput(io.Discard)
	}
	log.Printf("yarg called with args: %v\n", args)

	return
}

func main() {
	remainder := dispatchGlobalFlags(os.Args)
	dispatchSubCommand(remainder)
}
