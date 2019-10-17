# DE0 Nano FPGA/HPS Documentation

## Usage

- Connect to Linux system via ethernet
  - Static IP: 169.254.0.1
  - Users: yonder, root
  - Ask me for password
- Start C program to interface with FPGA
- Example program: `/home/root/counter`
  - Prints value of encoder count on change (signed 32-bit)
  - C code polls every 50ms but counting (decoding) is performed on FPGA (100MHz clock)
  - Channels mapped to two rightmost switches (SW0, SW1)

## Required Software

- Quartus Prime Lite (Tested 18.1): https://fpgasoftware.intel.com/
- SoC FPGA Embedded Development Suite Standard (Tested 18.1): https://fpgasoftware.intel.com/soceds/

## Todo

- Look into decreasing compilation time (loosening timing constraints)
- Communication with other devices
  - Serial or Ethernet?
  - C or Python?
  - ROS node?
- Test GPIO pins
  - Read/Write capability needed or just Read?

## Done

- Static IP
- Automatically flash FPGA on boot & reboot
- Test Verilog quadrature decoder (with switches representing channels A, B)
- Memory map arbitrary-size register inside FPGA

## Build Instructions

- ### RBF File

  1. Run Compiler & Fitter et al in Quartus (can take upwards of 10 minutes!)
  2. `File > Convert Programming Files...`
  3. Target: `.RBF` (Raw Binary File), Mode: `1-bit Passive Serial`
  4. Ensure MSEL switches are set to the following: 101110, where the first bit is switch 1 and the last is switch 6
  5. Copy file to `/etc/fpga/soc_system.rbf` on HPS system, will be automatically flashed on boot
  6. To force flashing immediately run `/etc/rc5.d/S99fpgaflash.sh start` on HPS system

- ### System Header

  1. Open SOC EDS Command Shell
  2. Run shell script `./generate_hps_qsys_header.sh` in project directory
  3. Copy `hps_0.h` header file to `hps-c` directory

- ### C Code

  1. Open SOC EDS Command Shell
  2. Navigate to `hps-c` directory
  3. Run Makefile with `make`
  4. Copy to `/home/root` (temporary) on HPS system
  5. Make executable with `chmod +x program`
  6. Run program `./program`

## Makefile

```Makefile
TARGET = counter

# All libraries included in project directory,
# should be able to compile C code from any device
CROSS_COMPILE = arm-linux-gnueabihf-
CFLAGS = -static -g -Wall -I./hwlib/include -Dsoc_cv_av
LDFLAGS = -g -Wall
CC = $(CROSS_COMPILE)gcc
ARCH = arm

build: $(TARGET)

$(TARGET): main.o
    $(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
    rm -f $(TARGET) *.a *.o *~
```

## Quadrature Decoder (Verilog)

```Verilog
module quadenc(clk, quadA, quadB, count);
input clk, quadA, quadB;
output [31:0] count;

reg [2:0] quadA_delayed, quadB_delayed;
always @(posedge clk) quadA_delayed <= {quadA_delayed[1:0], quadA};
always @(posedge clk) quadB_delayed <= {quadB_delayed[1:0], quadB};

wire enable = quadA_delayed[1] ^ quadA_delayed[2] ^ quadB_delayed[1] ^ quadB_delayed[2];
wire direction = quadA_delayed[1] ^ quadB_delayed[2];

reg [31:0] count;
always @(posedge clk) begin
    if (enable) begin
        if (direction)
            count <= count + 1;
        else
            count <= count - 1;
    end
end

endmodule
```

## Example Program C Code

```C
int main() {
    int memoryFileDescriptor;
    if ((memoryFileDescriptor = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    void *virtualMemoryBase = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, memoryFileDescriptor, HW_REGS_BASE);
    if (virtualMemoryBase == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(memoryFileDescriptor);
        return 1;
    }

    void *encoderOffset = virtualMemoryBase +
                          ((unsigned long)(ALT_LWFPGASLVS_OFST + PIO_MMAP_ENC0_BASE) &
                           (unsigned long)(HW_REGS_MASK));

    uint32_t lastCount = 0;
    while (true) {
        uint32_t encoderCount = *(uint32_t *)encoderOffset;
        if (encoderCount != lastCount) {
            printf("Count: %u\n", encoderCount);
            lastCount = encoderCount;
        }
        usleep(50 * 1000);
    }

    if (munmap(virtualMemoryBase, HW_REGS_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(memoryFileDescriptor);
        return 1;
    }

    close(memoryFileDescriptor);
    return 0;
}
```

## Startup Scripts

Located in `/etc/rc5.d`:

- ### `S99networkconfig.sh`

```Bash
#! /bin/sh -e

case "$1" in
    start)
        ifconfig eth0 169.254.0.1;;
    stop)
        ifdown eth0;;
    status)
        ifconfig eth0;;
    *)
        echo "Usage: $0 {start|stop|status}"
esac

exit 0
```

- ### `S99fpgaflash.sh`

```Bash
#! /bin/sh -e

case "$1" in
    start)
        echo 0 > /sys/class/fpga-bridge/fpga2hps/enable
        echo 0 > /sys/class/fpga-bridge/hps2fpga/enable
        echo 0 > /sys/class/fpga-bridge/lwhps2fpga/enable
        dd if=/etc/fpga/soc_system.rbf of=/dev/fpga0 bs=1M
        echo 1 > /sys/class/fpga-bridge/fpga2hps/enable
        echo 1 > /sys/class/fpga-bridge/hps2fpga/enable
        echo 1 > /sys/class/fpga-bridge/lwhps2fpga/enable;;
    status)
        echo "fpga:" `cat /sys/class/fpga/fpga0/status`
        echo "fpga2hps:" `cat /sys/class/fpga-bridge/fpga2hps/enable`
        echo "hps2fpga:" `cat /sys/class/fpga-bridge/hps2fpga/enable`
        echo "lwhps2fpga:" `cat /sys/class/fpga-bridge/lwhps2fpga/enable`;;
    *)
        echo "Usage: $0 {start|status}"
esac

exit 0
```
