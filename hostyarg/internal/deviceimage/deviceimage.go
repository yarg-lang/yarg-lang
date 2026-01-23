package deviceimage

import (
	"fmt"
	"io"
	"log"
	"os"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_uf2"
)

func AddFile(lfs littlefs.LittleFs, fileToAdd string) {

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

func ListFiles(fs littlefs.LittleFs, dirEntry string) {

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

func ReadFromUF2File(bd block_device.BlockMemoryDeviceWriter, filename string) (e error) {
	f, e := os.Open(filename)
	if e == nil {
		defer f.Close()
		pico_uf2.ReadFromUF2(f, bd)
	}
	return e
}

func WriteToUF2File(bd block_device.BlockMemoryDeviceReader, filename string) error {
	f, err := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if err != nil {
		log.Fatal(err)
	}
	defer f.Close()

	pico_uf2.WriteAsUF2(bd, f)

	return nil
}

func blockDeviceFromUF2(fsFilename string) littlefs.BdFS {
	device := block_device.NewBlockDevice()

	fs := littlefs.NewBdFS(device, littlefs.FLASHFS_BASE_ADDR, littlefs.FLASHFS_BLOCK_COUNT)

	ReadFromUF2File(fs.Storage, fsFilename)

	return *fs
}

func CmdLs(fsFilename, dirEntry string) {
	fs := blockDeviceFromUF2(fsFilename)
	defer fs.Close()

	lfs, _ := littlefs.Mount(fs.Cfg)
	defer lfs.Close()

	ListFiles(lfs, dirEntry)
}

func Cmdformat(fsFilename string) error {
	fs := blockDeviceFromUF2(fsFilename)
	defer fs.Close()

	result := littlefs.Format(fs.Cfg)
	if result == nil {
		WriteToUF2File(fs.Storage, fsFilename)
	}
	return result
}

func CmdAddFile(fsFilename, fileToAdd string) {
	fs := blockDeviceFromUF2(fsFilename)
	defer fs.Close()

	lfs, _ := littlefs.Mount(fs.Cfg)
	defer lfs.Close()

	AddFile(lfs, fileToAdd)

	WriteToUF2File(fs.Storage, fsFilename)
}
