#ifndef SPIFLASH_H
#define SPIFLASH_H

#include "gpio.h"

#include "disk.h"
#include "mbed.h"

// #include "DMA.h"

class SPIFlash_Impl;

#define SPIFLASH_SECTOR_SIZE	4L * 1024L // 4KB, the minimum erasable sector size

class SPIFlash : public MSD_Disk {
public:

    SPIFlash(PinName, PinName, PinName, PinName, PinName, PinName);
    virtual ~SPIFlash() {};

    virtual int disk_initialize();
    virtual int disk_write(const char *buffer, uint32_t block_number);
    virtual int disk_read(char *buffer, uint32_t block_number);
    virtual int disk_status();
    virtual int disk_sync();
    virtual uint32_t disk_sectors();
    virtual uint64_t disk_size();
    virtual uint32_t disk_blocksize();
    virtual bool disk_canDMA(void);
    virtual bool busy(void);

    int erase();

    void periodic();

protected:
	SPIFlash_Impl* _impl;

	unsigned int _sectors;
    volatile bool busyflag;

    void writepage(const char *buffer, uint32_t page_number);
    void readpages(char *buffer, uint32_t page_number, uint32_t pages);

    void loadsector(uint32_t sectorNumber);
    void storesector();

	bool isSectorLoaded;
	bool isSectorWritten;
	uint32_t loadedSectorNumber;
	char sectorBuffer[SPIFLASH_SECTOR_SIZE];
};

#endif
