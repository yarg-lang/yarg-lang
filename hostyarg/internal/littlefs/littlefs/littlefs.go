package littlefs

// littlefs provides two separate layers. A filesystem implementation that is compatible with littlefs images,
// and a set of callbacks (the Store interface) that store that image on a block/page based store, assumed to be like a flash device.
// Both co-exist in one go packge, so that the C littlefs namespace can be used by both.

/*
#include <stdlib.h>
#include "../../../littlefs/lfs.h"
#include "littlefs_utilities.h"
*/
import "C"
import (
	"errors"
	"io"
	"runtime"
	"unsafe"
)

type FS struct {
	chandle *C.lfs_t
	pins    runtime.Pinner
}

type FSInfo struct {
	DiskVersion uint32
	BlockSize   uint32
	BlockCount  uint32
	NameMax     uint32
	FileMax     uint32
	AttrMax     uint32
}

type entryType int8

const (
	EntryTypeReg entryType = C.LFS_TYPE_REG
	EntryTypeDir           = C.LFS_TYPE_DIR
)

type FileInfo struct {
	filetype entryType
	size     uint32
	name     string
}

type File struct {
	fs      *FS
	chandle *C.lfs_file_t
}

type Dir struct {
	fs      *FS
	chandle *C.lfs_dir_t
}

func (info FileInfo) Size() int64 {
	return int64(info.size)
}

func (info FileInfo) IsDir() bool {
	return info.filetype == EntryTypeDir
}

func (info FileInfo) Name() string {
	return info.name
}

// Format uses a config describing the desired filesystem. It is not mounted, so no Close or similar is needed.
func Format(config *Config) error {

	lfs := FS{chandle: &C.lfs_t{}}

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_format(lfs.chandle, config.lfs_config)
	if result < 0 {
		return errors.New("format failed")
	} else {
		return nil
	}
}

func Mount(cfg *Config) (*FS, error) {

	lfs := FS{chandle: &C.lfs_t{}}

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_mount(lfs.chandle, cfg.lfs_config)
	if result < 0 {
		return nil, errors.New("mount failed")
	} else {
		return &lfs, nil
	}
}

func (lfs *FS) unmount() error {

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_unmount(lfs.chandle)
	if result < 0 {
		return errors.New("unmount failed")
	} else {
		return nil
	}
}

func (lfs *FS) Close() error {
	return lfs.unmount()
}

func (lfs *FS) StatFS() (info FSInfo, err error) {

	fsinfo_handle := &C.struct_lfs_fsinfo{}

	lfs.pins.Pin(fsinfo_handle)
	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_fs_stat(lfs.chandle, fsinfo_handle)
	if result < 0 {
		return info, errors.New("statfs failed")
	} else {
		info.DiskVersion = uint32(fsinfo_handle.disk_version)
		info.BlockCount = uint32(fsinfo_handle.block_count)
		info.BlockSize = uint32(fsinfo_handle.block_size)
		info.NameMax = uint32(fsinfo_handle.name_max)
		info.FileMax = uint32(fsinfo_handle.file_max)
		info.AttrMax = uint32(fsinfo_handle.attr_max)

		return info, nil
	}
}

func (lfs *FS) Size() (size uint32, err error) {

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_fs_size(lfs.chandle)
	if result < 0 {
		return 0, errors.New("size failed")
	} else {
		return uint32(result), nil
	}
}

func (lfs *FS) SizeUsed() (size uint32, err error) {

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_fs_usage(lfs.chandle)

	if result < 0 {
		return 0, errors.New("size used failed")
	} else {
		return uint32(result), nil
	}
}

func (lfs *FS) Stat(name string) (info FileInfo, err error) {

	info_handle := &C.struct_lfs_info{}

	lfs.pins.Pin(info_handle)
	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	result := C.lfs_stat(lfs.chandle, cname, info_handle)
	if result < 0 {
		return info, errors.New("stat failed")
	}

	info.filetype = entryType(info_handle._type)
	info.size = uint32(info_handle.size)
	info.name = C.GoString(&info_handle.name[0])
	return info, nil
}

func (lfs *FS) Mkdir(name string) error {

	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_mkdir(lfs.chandle, C.CString(name))
	if result < 0 {
		return errors.New("mkdir failed")
	}
	return nil
}

func (lfs *FS) OpenDir(name string) (*Dir, error) {

	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	dir_handle := &C.lfs_dir_t{}

	lfs.pins.Pin(dir_handle)
	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	result := C.lfs_dir_open(lfs.chandle, dir_handle, cname)
	if result < 0 {
		return nil, errors.New("dir open failed")
	} else {
		return &Dir{fs: lfs, chandle: dir_handle}, nil
	}
}

func (lfs *FS) OpenFile(name string) (*File, error) {

	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	file_handle := &C.lfs_file_t{}

	lfs.pins.Pin(file_handle)
	lfs.pins.Pin(lfs.chandle)
	defer lfs.pins.Unpin()

	oflags := C.int(C.LFS_O_RDWR | C.LFS_O_CREAT)

	result := C.lfs_file_open(lfs.chandle, file_handle, cname, oflags)
	if result < 0 {
		return nil, errors.New("file open error")
	} else {
		return &File{fs: lfs, chandle: file_handle}, nil
	}
}

func (dir *Dir) Close() error {
	dir.fs.pins.Pin(dir.chandle)
	dir.fs.pins.Pin(dir.fs.chandle)
	defer dir.fs.pins.Unpin()

	result := C.lfs_dir_close(dir.fs.chandle, dir.chandle)
	if result < 0 {
		return errors.New("dir close failed")
	}
	return nil
}

func (dir *Dir) DirRead() (more bool, info FileInfo, err error) {

	info_handle := &C.struct_lfs_info{}

	dir.fs.pins.Pin(info_handle)
	dir.fs.pins.Pin(dir.chandle)
	dir.fs.pins.Pin(dir.fs.chandle)
	defer dir.fs.pins.Unpin()

	result := C.lfs_dir_read(dir.fs.chandle, dir.chandle, info_handle)
	if result < 0 {
		return false, info, errors.New("dir read failed")
	} else {
		info.filetype = entryType(info_handle._type)
		info.size = uint32(info_handle.size)
		info.name = C.GoString(&info_handle.name[0])
		return result != 0, info, nil
	}
}

func (file *File) Close() error {

	file.fs.pins.Pin(file.chandle)
	file.fs.pins.Pin(file.fs.chandle)
	defer file.fs.pins.Unpin()

	result := C.lfs_file_close(file.fs.chandle, file.chandle)
	if result < 0 {
		return errors.New("file close error")
	}
	return nil
}

func (file *File) Write(data []byte) (int, error) {

	file.fs.pins.Pin(file.chandle)
	file.fs.pins.Pin(file.fs.chandle)
	defer file.fs.pins.Unpin()

	cdata := C.CBytes(data)
	defer C.free(cdata)

	result := C.lfs_file_write(file.fs.chandle, file.chandle, cdata, C.lfs_size_t(len(data)))
	if result < 0 {
		return 0, errors.New("write failed")
	}
	return int(result), nil
}

func (file *File) Read(data []byte) (int, error) {

	file.fs.pins.Pin(file.chandle)
	file.fs.pins.Pin(file.fs.chandle)
	defer file.fs.pins.Unpin()

	cdata := C.CBytes(data)
	defer C.free(cdata)

	result := C.lfs_file_read(file.fs.chandle, file.chandle, cdata, C.lfs_size_t(len(data)))
	if result < 0 {
		return 0, errors.New("read failed")
	} else if result == 0 {
		return 0, io.EOF
	} else {
		copy(data, C.GoBytes(cdata, result))
		return int(result), nil
	}
}

func (file *File) Rewind() error {

	file.fs.pins.Pin(file.chandle)
	file.fs.pins.Pin(file.fs.chandle)
	defer file.fs.pins.Unpin()

	result := C.lfs_file_rewind(file.fs.chandle, file.chandle)
	if result < 0 {
		return errors.New("rewind failed")
	}
	return nil
}
