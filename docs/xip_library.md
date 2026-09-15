# XIP Library Format

cyarg targets microcontrollers which have read only firmware storage that is memory mapped, and design for execute-in-place usage.

A 'file format' is needed to describe the location of yarg code and resources in this XIP memory, and also to be available as a file when cyarg runs on a host OS.

The file format consists of a set of concatenated nodes, with a header and index to each node at the start.

Within this format, the first node (0) is guaranteed to be the index node.

Nodes can be added with a guaranteed alignment. If needed padding will be inserted after the previous node to obtain this alignment.

Node 1 is an optional (can be length 0) binary yarg file, that is intended to used during startup/bootstrapping.

Node 2 is the root of the name/directory structure to lookup named files with.
