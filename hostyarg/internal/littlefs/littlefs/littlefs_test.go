package littlefs

import (
	"fmt"
	"io/fs"
	"math"
	"os"
	"path/filepath"
	"testing"

	block_device "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/device"
	block_io "github.com/yarg-lang/yarg-lang/hostyarg/internal/block/io"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/littlefs_store"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/block/pico_memory"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_flash_device"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/pico_uf2"
	"github.com/yarg-lang/yarg-lang/hostyarg/internal/yarg_littlefs"
)

type UF2FS struct {
	config *Config
	file   *os.File
}

func (u *UF2FS) Close() error {
	u.file.Close()
	u.config.Close()
	return nil
}

func newUF2FS(uf2Path string) (uf2 *UF2FS, e error) {

	uf2 = &UF2FS{}

	cwd, _ := os.Getwd()
	tdata := filepath.Join(cwd, uf2Path)

	uf2.file, e = os.Open(tdata)
	if e != nil {
		return nil, fmt.Errorf("unexpected error: %w", &fs.PathError{Op: "open", Path: tdata, Err: fs.ErrInvalid})
	}

	device := block_device.NewBlockDevice(block_io.DeviceAddress(pico_flash_device.BaseAddr), &pico_memory.BlockDeviceStorage{})

	e = pico_uf2.ReadFromUF2(uf2.file, device)
	if e != nil {
		return nil, fmt.Errorf("unexpected error: %w", e)
	}

	store := littlefs_store.NewDevice(device, block_io.DeviceAddress(yarg_littlefs.BaseAddr))
	uf2.config = NewConfig(&store, 0)

	return uf2, nil
}

func TestMount(t *testing.T) {

	uf2fs, e := newUF2FS("../../../testdata/yarg-lang-pico-0.3.0.uf2")
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer uf2fs.Close()

	littlefs, e := Mount(uf2fs.config)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer littlefs.Close()
}

func TestFSStat(t *testing.T) {

	uf2fs, e := newUF2FS("../../../testdata/yarg-lang-pico-0.3.0.uf2")
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer uf2fs.Close()

	littlefs, e := Mount(uf2fs.config)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer littlefs.Close()

	info, e := littlefs.StatFS()
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}

	if info.DiskVersion != 0x20001 {
		t.Fatalf("unexpected disk version: %x", info.DiskVersion)
	}
	if (info.BlockSize != pico_flash_device.ErasePageSize) || (info.BlockCount != yarg_littlefs.BlockCount) {
		t.Fatalf("unexpected block size/count: %v blocks of size %v", info.BlockCount, info.BlockSize)
	}
	if info.NameMax != math.MaxUint8 {
		t.Fatalf("unexpected name max: %v", info.NameMax)
	}
	if info.FileMax != math.MaxInt32 {
		t.Fatalf("unexpected file max: %v", info.FileMax)
	}
	if info.AttrMax != 1022 {
		t.Fatalf("unexpected attr max: %v", info.AttrMax)
	}
}

func TestFSSize(t *testing.T) {

	uf2fs, e := newUF2FS("../../../testdata/yarg-lang-pico-0.3.0.uf2")
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer uf2fs.Close()

	littlefs, e := Mount(uf2fs.config)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer littlefs.Close()

	info, e := littlefs.Size()
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}

	if info != 28 {
		t.Fatalf("unexpected size: %v", info)
	}
}

func TestFSSizeUsed(t *testing.T) {

	uf2fs, e := newUF2FS("../../../testdata/yarg-lang-pico-0.3.0.uf2")
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer uf2fs.Close()

	littlefs, e := Mount(uf2fs.config)
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}
	defer littlefs.Close()

	info, e := littlefs.SizeUsed()
	if e != nil {
		t.Fatalf("unexpected error: %v", e)
	}

	if info != 28 {
		t.Fatalf("unexpected size used: %v", info)
	}
}
