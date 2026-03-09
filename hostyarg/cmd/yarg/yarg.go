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
		devicePort := flags.String("port", "", "serial port to use as device console")
		localRunInterpreter := flags.String("interpreter", "", "default interpreter")
		flags.Parse(args[1:])
		positionals := flags.Args()
		if len(positionals) != 1 {
			exitWithUsageError("unexpected positional arguments: " + fmt.Sprint(positionals))
		}
		if positionals[0] == "" {
			exitWithUsageError("expect source to run")
		}

		runner, err := hostyarg.DefaultYargRunner(*devicePort, *localRunInterpreter)
		if err != nil {
			exitWithError(err.Error())
		}

		err = runner.Run(positionals[0])
		if err != nil {
			exitWithError(err.Error())
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
		hostyarg.CmdListDevices()
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
