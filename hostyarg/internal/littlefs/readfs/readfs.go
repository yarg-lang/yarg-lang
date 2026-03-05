package readfs

// readfs implements a read-only fs.FS interface over littlefs.

import (
	"io/fs"
	"path/filepath"
	"strings"
	"time"

	"github.com/yarg-lang/yarg-lang/hostyarg/internal/littlefs/littlefs"
)

type FS struct {
	*littlefs.FS
	modtime time.Time
}

type File struct {
	file *littlefs.File
	dir  *littlefs.Dir
	info littlefs.FileInfo
	fs   *FS
}

type FileInfo struct {
	littlefs.FileInfo
	modtime time.Time
}

// fake up unix like permissions for littlefs entries.
const (
	dirModePermissions  = fs.ModeDir | 0755
	fileModePermissions = 0644
)

func (info FileInfo) Mode() fs.FileMode {
	if info.IsDir() {
		return dirModePermissions
	} else {
		return fileModePermissions
	}
}

func (info FileInfo) Type() fs.FileMode {
	return info.Mode() & fs.ModeType
}

func (info FileInfo) Info() (fs.FileInfo, error) {
	return info, nil
}

func (info FileInfo) ModTime() time.Time {
	return info.modtime
}

func (info FileInfo) Sys() any {
	return nil
}

func NewReadFS(cfg *littlefs.Config, modtime time.Time) (lfs *FS, e error) {
	impl, e := littlefs.Mount(cfg)
	if e != nil {
		return
	}

	lfs = &FS{FS: impl, modtime: modtime}
	return
}

func (lfs *FS) Close() error {
	return lfs.FS.Close()
}

func (lfs *FS) normalizePath(path string) (string, error) {
	if !fs.ValidPath(path) {
		return "", fs.ErrInvalid
	}

	// Clean the path to remove redundant separators and resolve . components
	cleaned := filepath.Clean(path)

	// Convert back slashes to forward slashes for consistency
	cleaned = filepath.ToSlash(cleaned)

	// Ensure we don't have any parent directory references
	if strings.Contains(cleaned, "..") {
		return "", fs.ErrInvalid
	}

	return cleaned, nil
}

func (lfs *FS) Open(name string) (fs.File, error) {

	normalisedName, err := lfs.normalizePath(name)
	if err != nil {
		return nil, &fs.PathError{Op: "open", Path: name, Err: fs.ErrInvalid}
	}

	info, err := lfs.FS.Stat(normalisedName)
	if err != nil {
		return nil, err
	}

	if info.IsDir() {
		dir, err := lfs.FS.OpenDir(normalisedName)
		if err != nil {
			return nil, err
		}
		return &File{fs: lfs, dir: dir, info: info}, nil
	} else {
		file, err := lfs.FS.OpenFile(normalisedName)
		if err != nil {
			return nil, err
		}
		return &File{fs: lfs, file: file, info: info}, nil
	}
}

func (f *File) Close() error {
	if f.info.IsDir() {
		return f.dir.Close()
	} else {
		return f.file.Close()
	}
}

func (f *File) Stat() (fs.FileInfo, error) {
	return &FileInfo{FileInfo: f.info, modtime: f.fs.modtime}, nil
}

func (f *File) Read(p []byte) (n int, err error) {
	if f.info.IsDir() {
		return 0, fs.ErrInvalid
	}
	return f.file.Read(p)
}

func (f *File) ReadDir(n int) ([]fs.DirEntry, error) {
	if !f.info.IsDir() {
		return nil, fs.ErrInvalid
	}

	dirs := make([]fs.DirEntry, 0)

	for more, info, err := f.dir.DirRead(); more && n < 0 || more && n > 0; more, info, err = f.dir.DirRead() {
		if err != nil {
			return nil, err
		}
		if (info.Name() == "." || info.Name() == "..") && info.IsDir() {
			continue
		}

		dirs = append(dirs, FileInfo{FileInfo: info, modtime: f.fs.modtime})
		if n > 0 {
			n--
		}
	}
	return dirs, nil
}
