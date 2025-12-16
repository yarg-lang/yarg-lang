Enhance testing with simulated threads and interrupt service routines

yarg test intrinsics:

test_set(address, value) // initialises I/O memory
test_read(address, [opt] value) // fails if a peek doesn’t match value or location is uninitialised
test_write(address, [opt] value) // fails if value
test_interrupt(number|string)
array[string] test_sync() // fails if unexpected peek/poke or missing read/write, returns empty if tests pass

So tests in yarg might be:
==============
test_set(@x1, 1);
test_set(@x2, 2);
test_write(@x3);
test_write(@x3, 3);
test_write(@x4); // res[6]
test_read(@x5, 5);
test_read(0x5); // res[4]
test_read(0x5); // res[5]
peek(@x1); // res[0]
poke @x3, 3;
poke @x3, 4;
peek(@x5); // res[1]
poke @x5, 5;
peek(@x5); // res[2]
var res = test_sync();
print res;

// The output looks like
//  peek(0x1) -> 1
//  poke 0x00000003, 0x00000003
//  poke 0x00000003, 0x00000004
//  peek(0x5) -> a5a5a5
//  poke 0x00000005, 0x00000005
//  Waiting for interrupts to be simulated - done
//  Type:any[7]:[missing test_read(@x000001);, missing poke/test_set @x000005, missing test_write(@x000005, 5);, 3 unfulfilled expectations:, test_read(@x000005);, test_read(@x000005); test_write(@x000004);]
==============
fun handler1() {print "handle1";}
var handler_routine1 = make_routine(handler1, true);
var handler_address1 = pin(handler_routine1);
irq_add_shared_handler(1, handler_address1, 1);
test_interrupt(1);
print test_sync();
irq_remove_handler(1, handler_address1);

// The output looks like
//  Waiting for interrupts to be simulated - handle1
//  done
//  Type:any[]:[]
==============

Further work:
ARM interrupts are not re-entrant. Should the test system ensure that there are not multiple test_interrupt() calls with the same interrupt id? What about other systems?
Should interrupts be started in order of lowest to highest priority to better simulate ARM?
Should the test system enforce
Simulate file system using xcode project

