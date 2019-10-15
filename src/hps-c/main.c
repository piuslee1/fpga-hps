#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps_0.h"

#define HW_REGS_BASE (ALT_STM_OFST)
#define HW_REGS_SPAN (0x04000000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)

int main()
{
    int memoryFileDescriptor;
    if ((memoryFileDescriptor = open("/dev/mem", O_RDWR | O_SYNC)) == -1)
    {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    void *virtualMemoryBase = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, memoryFileDescriptor, HW_REGS_BASE);
    if (virtualMemoryBase == MAP_FAILED)
    {
        printf("ERROR: mmap() failed...\n");
        close(memoryFileDescriptor);
        return 1;
    }

    void *encoderOffset = virtualMemoryBase +
                          ((unsigned long)(ALT_LWFPGASLVS_OFST + PIO_MMAP_ENC0_BASE) &
                           (unsigned long)(HW_REGS_MASK));

    uint32_t lastCount = 0;
    while (true)
    {
        uint32_t encoderCount = *(uint32_t *)encoderOffset;
        if (encoderCount != lastCount)
        {
            printf("Count: %u\n", encoderCount);
            lastCount = encoderCount;
        }
        usleep(50 * 1000);
    }

    if (munmap(virtualMemoryBase, HW_REGS_SPAN) != 0)
    {
        printf("ERROR: munmap() failed...\n");
        close(memoryFileDescriptor);
        return 1;
    }

    close(memoryFileDescriptor);
    return 0;
}
