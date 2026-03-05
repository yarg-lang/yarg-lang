package readfs

import (
	"fmt"
	"io/fs"
	"log"
	"os"
	"path/filepath"
	"testing"
	"time"

	block_device "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/device"
	block_io "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/io"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/littlefs_store"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/pico_memory"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs/littlefs"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_uf2"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/yarg_littlefs"
)

func TestRecursiveRead(t *testing.T) {

	cwd, _ := os.Getwd()
	tdata := filepath.Join(cwd, "../../../testdata/yarg-lang-pico-0.3.0.uf2")

	f, e := os.Open(tdata)
	if e != nil {
		t.Fatalf("unexpected error: %v", &fs.PathError{Op: "open", Path: tdata, Err: fs.ErrInvalid})
	}
	defer f.Close()

	device := block_device.NewBlockDevice(block_io.DeviceAddress(pico_flash_device.BaseAddr), &pico_memory.BlockDeviceStorage{})

	e = pico_uf2.ReadFromUF2(f, device)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}

	store := littlefs_store.NewDevice(device, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	config := littlefs.NewConfig(&store, 0)
	defer config.Close()

	lfs, e := NewReadFS(config, time.Now())
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer lfs.Close()

	err := fs.WalkDir(lfs, ".", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}

		fmt.Println(path)
		t.Log(path)
		return nil
	})

	if err != nil {
		log.Fatal(err)
	}
}
