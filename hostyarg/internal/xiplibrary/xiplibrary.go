package xiplibrary

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"io/fs"
	"log"
	"math"
	"os"
	"path/filepath"
)

type LibraryImageHeader struct {
	Magic          [6]byte
	Version        uint16
	Length         uint32
	NodeZeroOffset uint8
}

type LibraryNodeEntry struct {
	Length    uint32
	Alignment uint32
}

type LibraryWriter interface {
	io.WriteSeeker
	io.WriterAt
}

type LibraryDirEntry struct {
	FileNode uint16
	NameNode uint16
}

var packageMagic = [6]byte{0x79, 0x0a, 0x72, 0x67, 0xff, 0x43}

const packageVersion uint16 = 0x2600

var endianness = binary.LittleEndian

const nodeZeroAlignment = 4

func readLibraryImageHeader(data []byte) (header LibraryImageHeader, err error) {
	buf := bytes.NewReader(data)
	header = LibraryImageHeader{}
	err = binary.Read(buf, endianness, &header)
	if err != nil {
		return header, err
	}
	if header.Magic != packageMagic {
		return header, fmt.Errorf("invalid magic: %v", header.Magic)
	}
	if header.Version != packageVersion {
		return header, fmt.Errorf("invalid version: %v", header.Version)
	}
	if header.NodeZeroOffset == 0 {
		return header, fmt.Errorf("invalid node zero offset: %v", header.NodeZeroOffset)
	}

	return header, nil
}

func binaryWriteAt(w io.WriterAt, order binary.ByteOrder, data any, offset uint32) (err error) {
	buf := new(bytes.Buffer)
	err = binary.Write(buf, order, data)
	if err != nil {
		return err
	}
	_, err = w.WriteAt(buf.Bytes(), int64(offset))
	return err
}

func writeLibraryImageHeader(w io.WriterAt, length uint32) (err error) {

	headerSize := uint32(binary.Size(LibraryImageHeader{}))
	nodeZeroOffset := nodePadding(headerSize, nodeZeroAlignment) + headerSize
	if nodeZeroOffset > math.MaxUint8 {
		return fmt.Errorf("node zero offset too large: %v", nodeZeroOffset)
	}

	header := LibraryImageHeader{
		Magic:          packageMagic,
		Version:        packageVersion,
		Length:         length,
		NodeZeroOffset: uint8(nodeZeroOffset),
	}

	err = binaryWriteAt(w, endianness, header, 0)
	if err != nil {
		return err
	}
	return nil
}

func nodePadding(startLen uint32, alignment uint) uint32 {
	padding := uint32(alignment) - (startLen % uint32(alignment))
	if padding == uint32(alignment) {
		return 0
	}
	return padding
}

func writeLibraryPadding(w io.WriterAt, startLen uint32, alignment uint) (err error) {
	padding := nodePadding(startLen, alignment)
	if padding != 0 {
		_, err = w.WriteAt(make([]byte, padding), int64(startLen))
		return err
	}
	return nil
}

func writeStringNode(w LibraryWriter, s string, alignment uint) (err error) {
	c_string := append([]byte(s), 0)

	return writeLibraryNode(w, c_string, alignment)
}

func writeLibraryNode(w LibraryWriter, node []byte, alignment uint) (err error) {
	startLen64, err := w.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}
	startLen := uint32(startLen64)
	err = writeLibraryPadding(w, startLen, alignment)
	if err != nil {
		return err
	}
	paddedStartLen64, err := w.Seek(0, io.SeekEnd)
	if err != nil {
		return err
	}
	paddedStartLen := uint32(paddedStartLen64)
	dataLength := uint32(len(node))
	_, err = w.Write(node)
	err = writeLibraryImageHeader(w, paddedStartLen+dataLength)
	return err
}

func writeLibraryIndex(w LibraryWriter, lengths []LibraryNodeEntry) (err error) {

	indexNode := new(bytes.Buffer)
	indexOffset := uint32(16)
	indexLength := len(lengths)*8 + 8
	indexOffset += nodePadding(indexOffset, nodeZeroAlignment)

	binary.Write(indexNode, endianness, indexOffset)
	binary.Write(indexNode, endianness, uint32(indexLength))

	var offset uint32 = indexOffset + uint32(indexLength)
	for _, length := range lengths {
		offset += nodePadding(offset, uint(length.Alignment))
		err = binary.Write(indexNode, endianness, offset)
		if err != nil {
			return err
		}
		log.Printf("Offset: %d, Length: %d (alignment: %d, test %d)\n", offset, length.Length, length.Alignment, offset%uint32(length.Alignment))
		offset += length.Length
		err = binary.Write(indexNode, endianness, length.Length)
		if err != nil {
			return err
		}
	}
	return writeLibraryNode(w, indexNode.Bytes(), nodeZeroAlignment)
}

func writeDirectory(w LibraryWriter, directoryEntries []LibraryDirEntry) (err error) {
	dirNode := new(bytes.Buffer)
	for _, entry := range directoryEntries {
		err = binary.Write(dirNode, endianness, entry.FileNode)
		if err != nil {
			return err
		}
		err = binary.Write(dirNode, endianness, entry.NameNode)
		if err != nil {
			return err
		}
	}
	return writeLibraryNode(w, dirNode.Bytes(), 2)
}

func writeStartupFile(w LibraryWriter, startupFile string, alignment uint) (err error) {
	if startupFile == "" {
		return nil
	}
	data, err := os.ReadFile(startupFile)
	if err != nil {
		return err
	}
	return writeLibraryNode(w, data, alignment)
}

func CmdBuildLib(libDir, outputFile, startupFile string) error {
	libDir = filepath.Clean(libDir)
	outputFile = filepath.Clean(outputFile)

	libraryimage, err := os.Create(outputFile)
	if err != nil {
		return err
	}
	defer libraryimage.Close()

	filesystem := os.DirFS(libDir)

	entries, err := fs.ReadDir(filesystem, ".")
	if err != nil {
		return err
	}

	lengths := make([]LibraryNodeEntry, 0)

	if startupFile != "" {
		info, err := os.Stat(startupFile)
		if err != nil {
			return err
		}
		if info.Size() > math.MaxUint32 {
			return fmt.Errorf("startup file %s is too large", startupFile)
		}
		lengths = append(lengths, LibraryNodeEntry{Length: uint32(info.Size()), Alignment: 8})
	} else {
		lengths = append(lengths, LibraryNodeEntry{Length: 0, Alignment: 1})
	}

	directoryEntries := make([]LibraryDirEntry, 0)
	nodeCursor := uint16(3)
	for _, entry := range entries {
		info, err := entry.Info()
		if err != nil {
			return err
		}
		if info.IsDir() {
			continue
		}
		if info.Size() > math.MaxUint32 {
			return fmt.Errorf("file %s is too large", entry.Name())
		}
		lengths = append(lengths, LibraryNodeEntry{Length: uint32(info.Size()), Alignment: 8})
		lengths = append(lengths, LibraryNodeEntry{Length: uint32(len(entry.Name()) + 1), Alignment: 1})
		directoryEntries = append(directoryEntries, LibraryDirEntry{FileNode: nodeCursor, NameNode: nodeCursor + 1})
		nodeCursor += 2
	}

	err = writeLibraryImageHeader(libraryimage, uint32(binary.Size(LibraryImageHeader{})))
	if err != nil {
		return err
	}

	nodeLength := make([]LibraryNodeEntry, 0)
	nodeLength = append(nodeLength, lengths[0])
	nodeLength = append(nodeLength, LibraryNodeEntry{Length: uint32(len(directoryEntries)) * uint32(binary.Size(LibraryDirEntry{})), Alignment: 2})
	nodeLength = append(nodeLength, lengths[1:]...)

	err = writeLibraryIndex(libraryimage, nodeLength)
	if err != nil {
		return err
	}

	err = writeStartupFile(libraryimage, startupFile, uint(lengths[0].Alignment))
	if err != nil {
		return err
	}

	err = writeDirectory(libraryimage, directoryEntries)
	if err != nil {
		return err
	}

	lengthIndex := 1
	for _, entry := range entries {
		info, err := entry.Info()
		if err != nil {
			return err
		}
		if info.IsDir() {
			continue
		}
		if info.Size() > math.MaxUint32 {
			return fmt.Errorf("file %s is too large", entry.Name())
		}

		data, err := fs.ReadFile(filesystem, entry.Name())
		if err != nil {
			return err
		}
		err = writeLibraryNode(libraryimage, data, uint(lengths[lengthIndex].Alignment))
		if err != nil {
			return err
		}
		lengthIndex++
		err = writeStringNode(libraryimage, entry.Name(), uint(lengths[lengthIndex].Alignment))
		if err != nil {
			return err
		}
		lengthIndex++
	}
	return nil
}

func nodeDataBuffer(data []byte, nodeOffset uint32, nodeLength uint32) ([]byte, error) {
	if int(nodeOffset)+int(nodeLength) > len(data) {
		return nil, fmt.Errorf("node offset and length exceed contents length")
	}
	return data[nodeOffset : nodeOffset+nodeLength], nil
}

func nodeData(data []byte, node uint16) (nodeBytes []byte, err error) {
	header, err := readLibraryImageHeader(data)
	if err != nil {
		return nil, err
	}
	if header.Version != packageVersion {
		return nil, fmt.Errorf("unsupported library image version %d", header.Version)
	}

	if node == 0 {
		indexOffset := endianness.Uint32(data[header.NodeZeroOffset : header.NodeZeroOffset+4])
		indexLength := endianness.Uint32(data[header.NodeZeroOffset+4 : header.NodeZeroOffset+8])

		if indexOffset != uint32(header.NodeZeroOffset) {
			return nil, fmt.Errorf("index offset %d does not match header node zero offset %d", indexOffset, header.NodeZeroOffset)
		}

		return nodeDataBuffer(data, indexOffset, indexLength)
	} else {
		index, err := nodeData(data, 0)
		if err != nil {
			return nil, err
		}
		nodeCount, err := nodeCount(data)
		if err != nil {
			return nil, err
		}
		if int(node) >= nodeCount {
			return nil, fmt.Errorf("node %d out of range", node)
		}
		nodeOffset := endianness.Uint32(index[node*8 : node*8+4])
		nodeLength := endianness.Uint32(index[node*8+4 : node*8+8])
		return nodeDataBuffer(data, nodeOffset, nodeLength)
	}
}

func nodeCount(data []byte) (int, error) {
	index, err := nodeData(data, 0)
	if err != nil {
		return 0, err
	}
	return len(index) / 8, nil
}

func directories(data []byte) (dirs []LibraryDirEntry, err error) {
	numNodes, err := nodeCount(data)
	if err != nil {
		return nil, err
	}
	dirData, err := nodeData(data, 2)
	if err != nil {
		return nil, err
	}
	numEntries := len(dirData) / 4
	directoryEntries := make([]LibraryDirEntry, numEntries)
	for i := range directoryEntries {
		fileNode := endianness.Uint16(dirData[i*4 : i*4+2])
		nameNode := endianness.Uint16(dirData[i*4+2 : i*4+4])

		if fileNode >= uint16(numNodes) {
			return nil, fmt.Errorf("file node %d out of range", fileNode)
		}
		if nameNode >= uint16(numNodes) {
			return nil, fmt.Errorf("name node %d out of range", nameNode)
		}

		directoryEntries[i].FileNode = fileNode
		directoryEntries[i].NameNode = nameNode

	}
	return directoryEntries, nil
}

func CmdLs(fsFilename string, dirEntry string, long bool) (e error) {
	data, e := os.ReadFile(fsFilename)
	if e != nil {
		return e
	}

	_, e = readLibraryImageHeader(data)
	if e != nil {
		return e
	}

	directoryEntries, err := directories(data)
	if err != nil {
		return err
	}

	log.Printf("Directory Entries: %d\n", len(directoryEntries))

	for i, entry := range directoryEntries {
		fileNode := entry.FileNode
		nameNode := entry.NameNode

		nameData, err := nodeData(data, nameNode)
		if err != nil {
			return err
		}
		name := string(nameData[:len(nameData)-1])
		fileData, err := nodeData(data, fileNode)
		if err != nil {
			return err
		}
		log.Printf("Entry %d: File Node=%d, Name Node=%d, Name=%s (%d bytes)\n", i, fileNode, nameNode, name, len(fileData))

		if long {
			fmt.Printf("\t\t%d\t%s\n", len(fileData), name)
		} else {
			fmt.Printf("%s\n", name)
		}
	}

	return nil
}

type FsInfo struct {
	Version          uint16
	Size             uint32
	NodeCount        uint16
	UsefulLength     uint32
	DirectoryEntries uint16
}

func readFsInfo(data []byte) (FsInfo, error) {
	header, e := readLibraryImageHeader(data)
	if e != nil {
		return FsInfo{}, e
	}

	var info FsInfo
	info.Version = header.Version
	info.Size = header.Length
	numNodes, e := nodeCount(data)
	if e != nil {
		return FsInfo{}, e
	}
	info.NodeCount = uint16(numNodes)

	nodeZero, e := nodeData(data, 0)
	if e != nil {
		return FsInfo{}, e
	}

	var usefulLength uint32 = 0
	for i := range numNodes {
		nodeOffset := endianness.Uint32(nodeZero[i*8 : i*8+4])
		nodeLength := endianness.Uint32(nodeZero[i*8+4 : i*8+8])

		if int(nodeOffset)+int(nodeLength) > len(data) {
			return FsInfo{}, fmt.Errorf("node offset and length exceed contents length")
		}

		usefulLength += nodeLength
	}

	info.UsefulLength = uint32(usefulLength)

	nodeTwo, e := nodeData(data, 2)
	if e != nil {
		return FsInfo{}, e
	}
	info.DirectoryEntries = uint16(len(nodeTwo) / 4)

	return info, nil
}

func CmdFsInfo(fsFilename string) (e error) {
	data, e := os.ReadFile(fsFilename)
	if e != nil {
		return e
	}

	info, e := readFsInfo(data)
	if e != nil {
		return e
	}

	fmt.Printf("Library Version: %d\n", info.Version)
	fmt.Printf("Library Size: %d bytes\n", info.Size)
	fmt.Printf("Node Count: %d\n", info.NodeCount)
	fmt.Printf("Node Data: %d bytes\n", info.UsefulLength)
	fmt.Printf("Library Overhead: %d bytes\n", int(info.Size)-int(info.UsefulLength))
	fmt.Printf("Directory Entries: %d\n", info.DirectoryEntries)

	nodeOne, e := nodeData(data, 1)
	if e != nil {
		return e
	}
	if len(nodeOne) > 0 {
		fmt.Printf("Node 1 (Startup File) Size: %d bytes\n", len(nodeOne))
	} else {
		fmt.Printf("Node 1 (Startup File) not present\n")
	}

	return nil
}
