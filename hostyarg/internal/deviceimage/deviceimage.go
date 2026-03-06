package deviceimage

import (
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"time"

	block_device "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/device"
	block_io "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/io"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/littlefs_store"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/pico_memory"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs/littlefs"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs/readfs"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_uf2"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/yarg_littlefs"
)

func CopyFile(lfs *littlefs.FS, src, dest string) (e error) {
	srcFile, e := os.Open(src)
	if e != nil {
		return
	}
	defer srcFile.Close()

	destFile, e := lfs.OpenFile(dest)
	if e != nil {
		return
	}
	defer destFile.Close()

	_, e = io.Copy(destFile, srcFile)
	if e != nil {
		return
	}

	return
}

func ListFiles(filesystem fs.FS, path string, long bool) (err error) {

	entries, err := fs.ReadDir(filesystem, path)
	if err != nil {
		return
	}

	for _, entry := range entries {
		if long {
			info, err := entry.Info()
			if err != nil {
				return err
			}
			fmt.Printf("%s\t%d\t%s\n", info.Mode(), info.Size(), entry.Name())
		} else {
			fmt.Println(entry.Name())
		}
	}
	return
}

func writeToUF2File(bd block_io.BlockDeviceReader, filename string) (e error) {
	f, e := os.OpenFile(filename, os.O_RDWR|os.O_CREATE, 0666)
	if e != nil {
		return
	}
	defer f.Close()

	pico_uf2.WriteAsUF2(bd, f)

	return
}

func blockDeviceFromUF2(fsFilename string) (device block_device.BlockDevice, e error) {

	storage := pico_memory.BlockDeviceStorage{}

	device = block_device.NewBlockDevice(block_io.DeviceAddress(pico_flash_device.BaseAddr), &storage)

	f, e := os.Open(fsFilename)
	if e != nil {
		return
	}
	defer f.Close()

	e = pico_uf2.ReadFromUF2(f, device)
	return
}

func CmdLs(fsFilename, dirEntry string, long bool) (e error) {
	dev, e := blockDeviceFromUF2(fsFilename)
	if e != nil {
		return
	}
	defer dev.Close()

	store := littlefs_store.NewDevice(dev, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, 0)
	defer config.Close()

	lfs, e := readfs.NewReadFS(config, time.Now())
	if e != nil {
		return
	}
	defer lfs.Close()

	e = ListFiles(lfs, dirEntry, long)

	return
}

func CmdFsInfo(fsFilename string) (e error) {
	dev, e := blockDeviceFromUF2(fsFilename)
	if e != nil {
		return
	}
	defer dev.Close()

	store := littlefs_store.NewDevice(dev, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, 0)
	defer config.Close()

	lfs, e := readfs.NewReadFS(config, time.Now())
	if e != nil {
		return
	}
	defer lfs.Close()

	info, e := lfs.StatFS()
	if e != nil {
		return
	}

	blocks_available, e := lfs.Size()
	if e != nil {
		return
	}

	blocks_used, e := lfs.SizeUsed()
	if e != nil {
		return
	}

	fmt.Printf("Disk Version: %x\n", info.DiskVersion)
	fmt.Printf("Block Size: %d\n", info.BlockSize)
	fmt.Printf("Block Count: %d\n", info.BlockCount)
	fmt.Printf("Blocks Used: %d (%d bytes)\n", blocks_used, blocks_used*info.BlockSize)
	fmt.Printf("Free Space: %d blocks (%d bytes)\n", info.BlockCount-blocks_used, (info.BlockCount-blocks_used)*info.BlockSize)
	fmt.Printf("Data Available %d (nearest block, includes duplicated blocks)\n", blocks_available*info.BlockSize)
	fmt.Printf("Name Max: %d\n", info.NameMax)
	fmt.Printf("File Max: %d\n", info.FileMax)
	fmt.Printf("Attr Max: %d\n", info.AttrMax)

	return
}

func Cmdformat(fsFilename string, create bool) (e error) {

	fsFilename = filepath.Clean(fsFilename)

	creationRequired := false
	_, e = os.Stat(fsFilename)
	if os.IsNotExist(e) && create {
		creationRequired = true
	} else if e != nil {
		return
	}

	storage := pico_memory.BlockDeviceStorage{}
	device := block_device.NewBlockDevice(block_io.DeviceAddress(pico_flash_device.BaseAddr), &storage)
	defer device.Close()

	if !creationRequired {
		f, err := os.Open(fsFilename)
		if err != nil {
			return err
		}
		defer f.Close()

		e = pico_uf2.ReadFromUF2(f, device)
		if e != nil {
			return
		}
	}

	store := littlefs_store.NewDevice(device, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, yarg_littlefs.BlockCount)
	defer config.Close()

	e = littlefs.Format(config)
	if e != nil {
		return
	}

	if creationRequired {
		f, err := os.Create(fsFilename)
		if err != nil {
			return
		}
		defer f.Close()
	}

	e = writeToUF2File(device, fsFilename)
	return
}

func CmdCp(fsFilename, src, dest string) (e error) {
	dev, e := blockDeviceFromUF2(fsFilename)
	if e != nil {
		return
	}
	defer dev.Close()

	store := littlefs_store.NewDevice(dev, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, 0)
	defer config.Close()

	lfs, e := littlefs.Mount(config)
	if e != nil {
		return
	}
	defer lfs.Close()

	e = CopyFile(lfs, src, dest)
	if e != nil {
		return
	}

	writeToUF2File(dev, fsFilename)

	return
}

func CmdMkdir(fsFilename, dirToCreate string) (e error) {
	dev, e := blockDeviceFromUF2(fsFilename)
	if e != nil {
		return
	}
	defer dev.Close()

	store := littlefs_store.NewDevice(dev, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, 0)
	defer config.Close()

	lfs, e := littlefs.Mount(config)
	if e != nil {
		return
	}
	defer lfs.Close()

	e = lfs.Mkdir(dirToCreate)
	if e != nil {
		return
	}

	writeToUF2File(dev, fsFilename)

	return
}
