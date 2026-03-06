package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/deviceimage"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/devicerunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/hostrunner"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/testrunner"
	"mellium.im/sysexit"
)

func exitWithUsageError(usagenote string) {
	fmt.Println(usagenote)
	os.Exit(int(sysexit.ErrUsage))
}

func exitWithError(errnote string) {
	fmt.Println(errnote)
	os.Exit(1)
}

func dispatchSubCommand(args []string) {

	if len(args) < 1 {
		exitWithUsageError("expected command")
	}

	flags := flag.NewFlagSet(args[0], flag.ExitOnError)
	switch args[0] {
	case "format":
		formatFSFS := flags.String("fs", "", "format fs on this image")
		flags.Parse(args[1:])
		err := deviceimage.Cmdformat(*formatFSFS)
		if err != nil {
			exitWithUsageError("format failed")
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
	case "runtests":
		testRunInterpreter := flags.String("interpreter", "cyarg", "default interpreter")
		testRunTests := flags.String("tests", "", "default tests")
		flags.Parse(args[1:])

		err, failedtests := testrunner.CmdRunTests(*testRunInterpreter, *testRunTests)
		if err != nil {
			exitWithError(err.Error())
		}
		if failedtests > 0 {
			os.Exit(1)
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

		result := hostrunner.CmdCompile(*compileSource, *compileInterpreter)
		if !result {
			exitWithError("compile failed")
		}
	case "run":
		deviceRunSource := flags.String("source", "", "source to run on device")
		devicePort := flags.String("port", "", "serial port to use as device console")
		localRunInterpreter := flags.String("interpreter", "", "default interpreter")
		flags.Parse(args[1:])
		if *deviceRunSource == "" {
			exitWithUsageError("expect source to run")
		}
		if *devicePort == "" && *localRunInterpreter == "" {
			var ok bool
			*devicePort, ok = devicerunner.DefaultPort()
			if !ok {
				exitWithError("no valid port found")
			}
		}

		if *devicePort != "" {
			result := devicerunner.CmdRunOnDevice(*deviceRunSource, *devicePort)
			if result != nil {
				exitWithError("run failed")
			}
		} else {
			result := hostrunner.CmdRunLocally(*deviceRunSource, *localRunInterpreter)
			if result != nil {
				exitWithError("run failed")
			}
		}

	case "test":
		testRunInterpreter := flags.String("interpreter", "cyarg", "default interpreter")
		testRunSource := flags.String("source", "", "source of test to run")
		flags.Parse(args[1:])

		err, failedtests := testrunner.CmdRunTests(*testRunInterpreter, *testRunSource)
		if err != nil {
			exitWithError(err.Error())
		}
		if failedtests > 0 {
			os.Exit(1)
		}
	case "devices":
		flags.Parse(args[1:])
		devicerunner.CmdListDevices()
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
