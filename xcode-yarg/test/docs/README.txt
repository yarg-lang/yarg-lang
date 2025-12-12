Enhance testing with simulated threads and interrupt service routines

yarg test intrinsics:

test_set(address, value) // initialises memory
test_read(address, [opt] value) // fails if a peek doesn’t match  value or initialised
test_write(address, [opt] value) // fails if value
test_interrupt(number|string)
list<string> test_sync() // fails if unexpected peek/poke or missing read/write, returns empty if tests pass

So a test in yarg might be:
==============
test_read(a)
test_read(a)
test_read(a, x)
test_write(a, y) // note - test would pass if w is poked then y - to ensure order use sync
test_write(a, w)

test_read(b) // note - test would pass if b is poked then peeked and v.v. - to ensure order use sync
test_write(b)
test_write(d)
test_write(d)
test_write(d)
test_read(c) // note - test would pass if c is peeked before or after b - to ensure order use sync

test_set(a)
test_set(b)
test_set(c)
test_interrupt(1)

res = test_sync()
// remove any expected spurious reads/writes
res.delete_matching_regex("Unexpected read of 0x122122*\n");

test_read() …
test_write()…

test_set(a)
test_set(e)
test_set(f)
test_interrupt(1)
test_interrupt("sio2") // note interrupt are simulated on a thread pool, so may occur in either order

res += test_sync()

assert(res.empty())
==============

Further work:
Simulate file system using xcode project

