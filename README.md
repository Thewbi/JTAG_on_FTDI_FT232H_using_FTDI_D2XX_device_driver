# JTAG_on_FTDI_FT232H_using_FTDI_D2XX_device_driver
FT232H JTAG example code from the AN 129 with extended comments

Sample FTDI projects:

https://ftdichip.com/software-examples/mpsse-projects/

MPSSE Specification:

https://www.ftdichip.com/Support/Documents/AppNotes/AN2232C-01_MPSSE_Cmnd.pdf



# FT232H

## FT232H JTAG commands

### Commands 0x4A, 0x4B (JTAG TMS write but no data read)

0x4A, 0x4B - The 0x4A, 0x4B commands will only write to DATA OUT (TDO) and TMS
but they will not read data! See page 16 which states: "No read operation will take place".

0x4A - sends data on the rising edge of the JTAG clock.
0x4B - sends data on the falling edge of the JTAG clock.

The question that remains is if you want the FT232H to send data on the rising or on the falling edge.

The description of the SN74BCT8244ADW chip used in the FT232H example code states that:
"Data is captured on the rising edge of TCK and outputs change on the falling edge of TCK."
We will follow this convention. Data is therefore sent in on the rising edge and read
on the falling edge. This means of the two options (0x4A, 0x4B) we have, we will use
0x4A because it sends on the rising edge.

### Commands 0x6A, 0x6B, 0x6E, 0x6F (JTAG TMS write AND data read)

Out of all options

0x6A : TMS with LSB first on +ve clk edge, read on +ve edge - use if clk is set to '1'
0x6B : TMS with LSB first on -ve clk edge, read on +ve edge - use if clk is set to '0'
0x6E : TMS with LSB first on +ve clk edge, read on -ve edge - use if clk is set to '1'
0x6F : TMS with LSB first on -ve clk edge, read on -ve edge - use if clk is set to '0'

0x6E seems to be exactly what we need as it writes on the rising edge and it will read on
the falling edge.

0x6E : TMS with LSB first on +ve clk edge, read on -ve edge - use if clk is set to '1'

Example:
Assume that the current state is SHIFT_IR and assume that our wish is to read a singe bit
from the IR register.

To write data, all pins need to be properly connected:

TCK is pin AD0
TDI is pin AD1
TDO is pin AD2
TMS is pin AD3



### Writing a 32 bit values

There is quite a big possibility that I have not grasped the genious that is the FT232H API as provided by the FT232H lib!
But the parts that I have seen so far are hard to understand.

Here is how to read and write a 32 bit value into a 32 bit register using TDI.
First of all, TDI on the FT232H breakout board has to be connected to TDI on the target! TDO likewise.

To send the 32 bit value, the only option you have is to perform 32 calls to the command 0x6E because per 0x6E command, 
there is only a single bit of space for data and 7 bits for the TMS signal! The provided bit of data is applied to all
TMS values alike!

I do not understand the API concept. Why is there more space for TMS than for data? Why is there not API where I can just 
tell the device to send a byte of data? And also what if I want to send different data per TMS value? What kind of sense
does that API make? The only situation in which this separation of data and TMS makes sense is if you want to keep clocking
in 1s or 0s for an extended amount of TMS signals! When is this the case ever?

Anyways, you will clock in your 32 bit value bit-wise. At the same time, the 0x6E command will shift out a single bit to TDO
for each bit you shift into TDI. Each shift operation alters an internal 8 bit (1 byte) buffer. Per shift operation, that
snapshot of the internal 8 bit buffer is placed into the Queue. You can then read all snapshots from the queue using
FT_GetQueueStatus() to see how many snapshots there are and using FT_Read() to read all the snapshots in one go (if you like).

This means that for a 32 bit value that is shifted in bit-wise, the queue will contain 32 snapshots. Each snapshot takes 1 Byte
of space in the queue.

For shifting a value into the register, the shifted out bits might have no meaning. They are basically the old value of the 
register that you have overriden using the write operation. If you care about the old value, you need to reassemble the value
from the snapshots.

Lets break the discussion up in a pure read() operation and a pure write() operation. The write() operation does not care
about the old value that is shifted out during write and it will not interpret the snapshots.

### Reading a 32 bit value

Now lets talk about the pure read() operation. For the read operation, the same principle applies. You need to shift
in bits into the register to get bits shifted out. The out-shifted bits will appear in the form of snapshots. You have
to reassemble the value given the snapshots in the queue! The command to read data is also 0x6E (or one of the variants
depending on which edge is used to supply and produce data).

This time, if we talk about a pure Read() operation, we can shift i 0 values or 1 values. Basically it does not matter
which data we shift in since the pure Read() is only interested about the snapshot coming out of the register.
Say we shift in 0 data in order to retrieve the register content.
This time, in contrast to the Write() operation, there is the possibility to shift out up to 7 values at a time since 
we can provide the 0 data and 7 values for TMS. Then the FT232h will clock in the 7 TMS values and it will give us
seven shifted bits!

In terms of snapshots, for a 32 bit value, you have to shift 7-bit, followed by 7-bit, followed by 7-bit and 7-bit and 4-bit.
7+7+7+7+4 = 32 bit.

This means, you will receive 5 snapshots in the queue.

Here is an example:

Let's say the 32-bit register is preloaded with the value 0x12345678.
Shifting our 7+7+7+7+4 combo of awesomnes yields the snapshots:

```
0x12     0x23     0xA2     0x59     0xF0
```

Lets display the hex numbers as binary numbers:

```
00010010 00100011 10100010 01011001 11110000
```

Now you need to know which snapshot has been created by how many bit shifts!
The wonderfull API does not tell you that at all, you have to remember.
In our case the combo is fixed for 32 bit values, it is 7+7+7+7+4 so we can
hardcode this in the routine.

Lets hightlight which bits within the snapshots contain actual data and which bits are left-over data from the last shift:

```
[0001]0010 [0010001]1 [1010001]0 [0101100]1 [1111000]0
```

The snapshot on the right has seven bits from the value 0x78 from the original register content shifted in from the left!
0x78 == 01111000. When you shift 01111000 seven times to the right, you get 1111000. When this value is shifted into the
8 bit buffer from the right, you get 11110000 and this is the value stored in the snapshot. Only the leftmost 7 bit are
real and they are marked in the display above.

Likewise, the rest of the 5 snapshots need to interpreted. Now that we know which bits are real per snapshot, we need 
to combine them to get the original register value.

```
[0001] [0010001] [1010001] [0101100] [1111000]

 0001   0010001   1010001   0101100   1111000
 
00010010001101000101011001111000 == 0x12345678
```

So far so good, we go the original value back! First, unimportant bits are dropped, then the bits are combined and then
there is the original register content of 0x12345678.

An implementation of this reassembling of the bits is basically your pure Read() function!

Here are the recombinations:

```
uint32_t value = (((byInputBuffer[4] & 0xF0) >> 4) << 28) 
	| (((byInputBuffer[3] & 0xFE) >> 1) << 21) 
	| (((byInputBuffer[2] & 0xFE) >> 1) << 14) 
	| (((byInputBuffer[1] & 0xFE) >> 1) << 7) 
	| (((byInputBuffer[0] & 0xFE) >> 1) << 0);
```

For each snapshot, select the relevant bits using a bitwise AND operation, then shift the bits to the right to
get rid of the irrelevant bits. Then shift the bits up into their correct values inside the 32 bit number.

### Reading and writing at the same time

If you want to read and write 32 bit values at the same time, you are stuck with 1 bit writes and 32 read snapshots
because of the ridiculous 1 bit constraint of the 0x6E command!

The upside of this is that you get 32 snapshots, and each 8th snapshot (indexes 7, 15, 23 and 31) contains a byte
of read data! You need not drop bits, you can just shift these four snapshots together to form a 32 bit value.
The reason is as each individual bit is shifted into the 8-bit internal buffer, every eight shifts, the internal 
buffer contains a valid value!

### Summing things up!

This all is so terrible that I have a hunch I might be missing a huge part of the API. Maybe there are
methods to just read and write data that I have missed! If you know more about the FT232H API, please let 
me know.
