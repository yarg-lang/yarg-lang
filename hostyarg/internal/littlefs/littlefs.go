package littlefs

/*
#include <stdlib.h>
#include "lfs.h"
*/
import "C"
import (
	"errors"
	"io"
	"runtime"
	"unsafe"
)

type LittleFs struct {
	chandle *C.lfs_t
}

type LittleFsEntryType int8

const (
	EntryTypeReg LittleFsEntryType = C.LFS_TYPE_REG
	EntryTypeDir                   = C.LFS_TYPE_DIR
)

type LittleFsInfo struct {
	Type LittleFsEntryType
	Size uint32
	Name string
}

func newLittleFs() *LittleFs {
	var clfs C.lfs_t
	lfs := LittleFs{chandle: &clfs}
	return &lfs
}

func Mount(cfg *C.struct_lfs_config) (LittleFs, error) {

	lfs := *newLittleFs()

	var pin runtime.Pinner
	pin.Pin(lfs.chandle)
	pin.Pin(cfg)
	pin.Pin(cfg.context)
	defer pin.Unpin()

	result := C.lfs_mount(lfs.chandle, cfg)
	if result < 0 {
		return lfs, errors.New("mount failed")
	} else {
		return lfs, nil
	}
}

func (fs LittleFs) unmount() error {
	var pin runtime.Pinner
	pin.Pin(fs.chandle)
	defer pin.Unpin()

	result := C.lfs_unmount(fs.chandle)
	if result < 0 {
		return errors.New("unmount failed")
	} else {
		return nil
	}
}

func (fs LittleFs) Close() error {
	return fs.unmount()
}

func Format(cfg *C.struct_lfs_config) error {

	lfs := newLittleFs()

	var pin runtime.Pinner
	pin.Pin(lfs.chandle)
	pin.Pin(cfg)
	defer pin.Unpin()

	result := C.lfs_format(lfs.chandle, cfg)
	if result < 0 {
		return errors.New("format failed")
	} else {
		return nil
	}
}

type LfsDir struct {
	fs      LittleFs
	chandle *C.lfs_dir_t
}

func newLfsDir(lfs LittleFs) LfsDir {
	var cdir C.lfs_dir_t
	dir := LfsDir{fs: lfs, chandle: &cdir}
	return dir
}

func (fs LittleFs) OpenDir(name string) (LfsDir, error) {
	dir := newLfsDir(fs)
	err := dir.Open(name)
	return dir, err
}

func (dir LfsDir) Open(name string) error {
	var pin runtime.Pinner
	pin.Pin(dir.chandle)
	defer pin.Unpin()

	pin.Pin(dir.fs.chandle.cfg.context)
	pin.Pin(dir.fs.chandle)

	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	result := C.lfs_dir_open(dir.fs.chandle, dir.chandle, cname)
	if result < 0 {
		return errors.New("dir open failed")
	}
	return nil
}

func (dir LfsDir) Close() error {
	var pin runtime.Pinner
	pin.Pin(dir.chandle)
	defer pin.Unpin()
	pin.Pin(dir.fs.chandle)

	result := C.lfs_dir_close(dir.fs.chandle, dir.chandle)
	if result < 0 {
		return errors.New("dir close failed")
	}
	return nil
}

func (dir LfsDir) Read() (more bool, info LittleFsInfo, err error) {
	var pin runtime.Pinner
	pin.Pin(dir.chandle)
	defer pin.Unpin()
	pin.Pin(dir.fs.chandle)

	var cinfo C.struct_lfs_info

	result := C.lfs_dir_read(dir.fs.chandle, dir.chandle, &cinfo)
	if result < 0 {
		return false, info, errors.New("dir read failed")
	} else {
		info.Type = LittleFsEntryType(cinfo._type)
		info.Size = uint32(cinfo.size)
		info.Name = C.GoString(&cinfo.name[0])
		return result != 0, info, nil
	}
}

type LfsFile struct {
	fs      LittleFs
	chandle *C.lfs_file_t
}

func newLfsFile(lfs LittleFs) LfsFile {
	var f C.lfs_file_t
	var lf = LfsFile{fs: lfs, chandle: &f}

	return lf
}

func (fs LittleFs) OpenFile(name string) (LfsFile, error) {
	file := newLfsFile(fs)
	return file, file.Open(name)
}

func (file LfsFile) Open(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))

	var pin runtime.Pinner
	pin.Pin(file.chandle)
	defer pin.Unpin()
	pin.Pin(file.fs.chandle)

	oflags := C.int(C.LFS_O_RDWR | C.LFS_O_CREAT)

	result := C.lfs_file_open(file.fs.chandle, file.chandle, cname, oflags)
	if result < 0 {
		return errors.New("file open error")
	}
	return nil
}

func (file LfsFile) Close() error {
	var pin runtime.Pinner
	pin.Pin(file.chandle)
	defer pin.Unpin()
	pin.Pin(file.fs.chandle)

	result := C.lfs_file_close(file.fs.chandle, file.chandle)
	if result < 0 {
		return errors.New("file close error")
	}
	return nil
}

func (file LfsFile) Write(data []byte) (int, error) {

	var pin runtime.Pinner
	pin.Pin(file.chandle)
	defer pin.Unpin()
	pin.Pin(file.fs.chandle)

	cdata := C.CBytes(data)
	defer C.free(cdata)

	result := C.lfs_file_write(file.fs.chandle, file.chandle, cdata, C.lfs_size_t(len(data)))
	if result < 0 {
		return 0, errors.New("write failed")
	}
	return int(result), nil
}

func (file LfsFile) Read(data []byte) (int, error) {

	var pin runtime.Pinner
	pin.Pin(file.chandle)
	defer pin.Unpin()

	pin.Pin(file.fs.chandle)

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

func (file LfsFile) Rewind() error {

	var pin runtime.Pinner
	pin.Pin(file.chandle)
	defer pin.Unpin()

	pin.Pin(file.fs.chandle)

	result := C.lfs_file_rewind(file.fs.chandle, file.chandle)
	if result < 0 {
		return errors.New("rewind failed")
	}
	return nil
}
