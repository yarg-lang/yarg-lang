package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_uf2"
)

func add_file(lfs littlefs.LittleFs, fileToAdd string) {

	r, err := os.Open(fileToAdd)
	if err != nil {
		log.Fatal("nothing to open")
	}
	defer r.Close()
	data, err := io.ReadAll(r)
	if err != nil {
		log.Fatal("nothing read")
	}

	file, _ := lfs.OpenFile(fileToAdd)
	defer file.Close()

	file.Write(data)
}

func list_files(fs littlefs.LittleFs, dirEntry string) {

	dir, err := fs.OpenDir(dirEntry)
	if err != nil {
		log.Fatal(err.Error())
	}
	defer dir.Close()

	for more, info, err := dir.Read(); more; more, info, err = dir.Read() {
		if err != nil {
			log.Fatal(err)
		}
		switch info.Type {
		case littlefs.EntryTypeDir:
			fmt.Printf("'%v' (dir)\n", info.Name)
		case littlefs.EntryTypeReg:
			fmt.Printf("'%v' (%v)\n", info.Name, info.Size)
		default:
			log.Fatal("unexpected entry type.")
		}
	}

}

func readFromUF2File(bd block_device.BlockDevice, filename string) (e error) {
	f, e := os.Open(filename)
	if e == nil {
		defer f.Close()
		pico_uf2.ReadFromUF2(f, bd)
	}
	return e
}

func writeToUF2File(bd block_device.BlockDevice, filename string) error {
	f, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()

	pico_uf2.WriteAsUF2(bd, f)

	return nil
}

func formatCmd(fs littlefs.BdFS, fsFilename string) {
	readFromUF2File(fs.Flash_fs.Device, fsFilename)

	littlefs.Format(fs.Cfg)

	writeToUF2File(fs.Flash_fs.Device, fsFilename)
}

func cmdAddFile(fs littlefs.BdFS, fsFilename, fileToAdd string) {
	readFromUF2File(fs.Flash_fs.Device, fsFilename)

	lfs, _ := littlefs.Mount(fs.Cfg)
	defer lfs.Close()

	add_file(lfs, fileToAdd)

	writeToUF2File(fs.Flash_fs.Device, fsFilename)
}

func cmdLs(fs littlefs.BdFS, fsFilename, dirEntry string) {
	readFromUF2File(fs.Flash_fs.Device, fsFilename)
	lfs, _ := littlefs.Mount(fs.Cfg)
	defer lfs.Close()

	list_files(lfs, dirEntry)
}

func main() {

	log.SetOutput(io.Discard)

	formatFSCmd := flag.NewFlagSet("format", flag.ExitOnError)
	formatFSFS := formatFSCmd.String("fs", "test.uf2", "format fs on this image")

	addFileCmd := flag.NewFlagSet("addfile", flag.ExitOnError)
	addFileFS := addFileCmd.String("fs", "test.uf2", "add file to this filesystem")
	addFileName := addFileCmd.String("add", "", "filename to add")

	lsDirCmd := flag.NewFlagSet("ls", flag.ExitOnError)
	lsDirFS := lsDirCmd.String("fs", "test.uf2", "filesystem to mount")
	lsDirEntry := lsDirCmd.String("dir", "/", "directory to ls")

	testRunCmd := flag.NewFlagSet("runtests", flag.ExitOnError)
	testRunInterpreter := testRunCmd.String("interpreter", "cyarg", "default interpreter")
	testRunTests := testRunCmd.String("tests", "", "default tests")

	if len(os.Args) < 2 {
		fmt.Println("expected command")
		os.Exit(64)
	}

	device := block_device.NewBlockDevice()
	defer device.Close()

	fs := littlefs.NewBdFS(device, littlefs.FLASHFS_BASE_ADDR, littlefs.FLASHFS_BLOCK_COUNT)
	defer fs.Close()

	switch os.Args[1] {
	case "format":
		formatFSCmd.Parse(os.Args[2:])
		formatCmd(*fs, *formatFSFS)
	case "addfile":
		addFileCmd.Parse(os.Args[2:])
		if *addFileName == "" {
			fmt.Println("expect filename to add")
			os.Exit(64)
		}
		cmdAddFile(*fs, *addFileFS, *addFileName)
	case "ls":
		lsDirCmd.Parse(os.Args[2:])
		cmdLs(*fs, *lsDirFS, *lsDirEntry)
	case "runtests":
		testRunCmd.Parse(os.Args[2:])
		exit_code := cmdRunTests(*testRunInterpreter, *testRunTests)
		os.Exit(exit_code)
	default:
		fmt.Println("unknown command")
		os.Exit(64)
	}
}
