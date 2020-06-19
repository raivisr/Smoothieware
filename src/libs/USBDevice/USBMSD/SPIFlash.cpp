/* (c) 2016-07-01 Jens Hauke <jens.hauke@4k2.de> */
#include "SPIFlash.h"

#include <inttypes.h>

#define W25Q80BV_CMD_WRITE_ENABLE	0x06
#define W25Q80BV_CMD_PAGE_PROG	0x02
#define W25Q80BV_CMD_READ_DATA	0x03
#define W25Q80BV_CMD_READ_STAT1	0x05
#define W25Q80BV_MASK_STAT_BUSY	(1<<0)
#define W25Q80BV_CMD_ERASE_4K	0x20
#define W25Q80BV_CMD_ERASE_32K	0x52
#define W25Q80BV_CMD_ERASE_64K	0xD8
#define W25Q80BV_CMD_CHIP_ERASE	0xC7 /* alternative 0x60 */
#define W25Q80BV_CMD_READ_MAN_DEV_ID	0x90
#define W25Q80BV_CMD_READ_UNIQUE_ID	0x4B
#define W25Q_PAGE_SIZE	256
#define W25Q80BV_CAPACITY	(1L * 1024L * 1024L)
#define W25Q64JV_CAPACITY	(8L * 1024L * 1024L)

#define SPIFLASH_PAGE_SIZE	W25Q_PAGE_SIZE
#define BLOCK_SIZE	512

class SPIFlash_Impl {
	// SPI flash implementation for W25Q - based on https://github.com/jensh/spiflash/blob/14df969dd4ad7e1012d6626ca58c62fe1d0de9c3/spiflash.c
public:
	mbed::SPI _spi;
	GPIO _cs;
	GPIO _hold;
	GPIO _wp;

	SPIFlash_Impl(PinName mosi, PinName miso, PinName sclk, PinName cs, PinName hold, PinName wp)
		: _spi(mosi, miso, sclk), _cs(cs), _hold(hold), _wp(wp)
	{
		_cs.output();
		_cs = 1;

		_hold.output();
		_hold = 1;

		_wp.output();
		_wp = 1;
	}

	void spiflash_end(void) {
		_cs = 1;

		// Force a single SPI clock cycle to ensure that the change to ~CS is seen.
		_spi.write(0xFF);
		_spi.write(0xFF);
	}

	inline uint32_t spiflash_is_present(void) {
		if (spiflash_device_id() == 0xEF13) {
			return true;
		} else if (spiflash_device_id() == 0xEF16) {
			return true;
		} else {
			return false;
		}
	}

	inline uint32_t spiflash_capacity(void) {
		if (spiflash_device_id() == 0xEF13) {
			return W25Q80BV_CAPACITY;
		} else if (spiflash_device_id() == 0xEF16) {
			return W25Q64JV_CAPACITY;
		} else {
			return ~0L;
		}
	}

	inline void spiflash_write_uint8(uint8_t val) {
		_spi.write(val);
	}

	inline void spiflash_write_end(void) {
		spiflash_end_wait();
	}

	inline uint8_t spiflash_read(void) {
		return spiflash_read_uint8();
	}


	inline void spiflash_read_end(void) {
		spiflash_end();
	}

	uint8_t spiflash_is_busy(void) {
		uint8_t res;
		spiflash_cmd_start(W25Q80BV_CMD_READ_STAT1);
		res = (spiflash_read_uint8() & W25Q80BV_MASK_STAT_BUSY);
		spiflash_end();
		return res;
	}


	// Wait until not busy
	void spiflash_wait(void) {
		spiflash_cmd_start(W25Q80BV_CMD_READ_STAT1);
		// ToDo: Timeout?
		while (spiflash_read_uint8() & W25Q80BV_MASK_STAT_BUSY);
		spiflash_end();
	}


	void spiflash_end_wait(void) {
		_cs = 1;

		// Force a single SPI clock cycle to ensure that the change to ~CS is seen.
		_spi.write(0xFF);
		_spi.write(0xFF);

		spiflash_wait();
		spiflash_end();
	}


	void spiflash_cmd_start(uint8_t cmd) {
		_cs = 0;
		_spi.write(cmd);
	}


	void spiflash_cmd_addr_start(uint8_t cmd, uint32_t addr) {
		spiflash_cmd_start(cmd);
		_spi.write((uint8_t)(addr >> 16));
		_spi.write((uint8_t)(addr >>  8));
		_spi.write((uint8_t)(addr));
	}


	inline void spiflash_write_enable(void) {
		spiflash_cmd_start(W25Q80BV_CMD_WRITE_ENABLE);
		spiflash_end();
	}


	void spiflash_write_start(uint32_t addr) {
		spiflash_write_enable();
		spiflash_cmd_addr_start(W25Q80BV_CMD_PAGE_PROG, addr);
	}


	void spiflash_read_start(uint32_t addr) {
		spiflash_cmd_addr_start(W25Q80BV_CMD_READ_DATA, addr);
	}


	uint8_t spiflash_read_uint8(void) {
		return _spi.write(0);
	}


	uint16_t spiflash_read_uint16(void) {
		return (uint16_t)spiflash_read_uint8() << 8 | spiflash_read_uint8();
	}


	uint32_t spiflash_read_uint32(void) {
		return (uint32_t)spiflash_read_uint16() << 16 | spiflash_read_uint16();
	}


	uint16_t spiflash_read_uint16_le(void) {
		return spiflash_read_uint8() | (uint16_t)spiflash_read_uint8() << 8;
	}


	uint32_t spiflash_read_uint32_le(void) {
		return spiflash_read_uint16_le() | (uint32_t)spiflash_read_uint16_le() << 16;
	}


	void spiflash_write_uint16(uint16_t val) {
		spiflash_write_uint8(val >> 8);
		spiflash_write_uint8(val);
	}


	void spiflash_write_uint32(uint32_t val) {
		spiflash_write_uint16(val >> 16);
		spiflash_write_uint16(val);
	}


	void spiflash_write_uint16_le(uint16_t val) {
		spiflash_write_uint8(val);
		spiflash_write_uint8(val >> 8);
	}


	void spiflash_write_uint32_le(uint32_t val) {
		spiflash_write_uint16_le(val);
		spiflash_write_uint16_le(val >> 16);
	}


	void spiflash_erase4k(uint32_t addr) {
		spiflash_write_enable();
		spiflash_cmd_addr_start(W25Q80BV_CMD_ERASE_4K, addr);
		spiflash_end_wait();
	}


	void spiflash_erase32k(uint32_t addr) {
		spiflash_write_enable();
		spiflash_cmd_addr_start(W25Q80BV_CMD_ERASE_32K, addr);
		spiflash_end_wait();
	}


	void spiflash_erase64k(uint32_t addr) {
		spiflash_write_enable();
		spiflash_cmd_addr_start(W25Q80BV_CMD_ERASE_64K, addr);
		spiflash_end_wait();
	}


	void spiflash_chip_erase(void) {
		spiflash_write_enable();
		spiflash_cmd_start(W25Q80BV_CMD_CHIP_ERASE);
		spiflash_end_wait();
	}


	uint16_t spiflash_device_id(void) {
		uint16_t id;
		spiflash_cmd_addr_start(W25Q80BV_CMD_READ_MAN_DEV_ID, 0x0000);
		id = spiflash_read_uint16();
		spiflash_end();
		//printf("spiflash_device_id = %d\n", id);
		return id;
	}


	uint64_t spiflash_unique_id(void) {
		uint64_t id;
		spiflash_cmd_addr_start(W25Q80BV_CMD_READ_UNIQUE_ID, 0x0000);
		_spi.write(0);
		id = (uint64_t)spiflash_read_uint32() << 32;
		id |= spiflash_read_uint32();
		spiflash_end();
		return id;
	}
};

SPIFlash::SPIFlash(PinName mosi, PinName miso, PinName sclk, PinName cs, PinName hold, PinName wp)
	: _impl(new SPIFlash_Impl(mosi, miso, sclk, cs, hold, wp))
    , busyflag(false)
	, isSectorLoaded(false)
	, isSectorWritten(false)
	, loadedSectorNumber(0)
{
    
}

bool SPIFlash::busy(void)
{
	return busyflag;
}

int SPIFlash::disk_initialize()
{
    _impl->_spi.frequency(2500000);

	_sectors = _impl->spiflash_capacity() / SPIFLASH_PAGE_SIZE;

    return _impl->spiflash_is_present() ? 0 : 1;
}

int SPIFlash::erase()
{
	busyflag = true;
	_impl->spiflash_chip_erase();
	busyflag = false;
    return 0;
}

void SPIFlash::writepage(const char *buffer, uint32_t page_number)
{
	//printf("SPIFlash::writepage(%p, %u)\n", buffer, page_number);

	_impl->spiflash_write_start(page_number * SPIFLASH_PAGE_SIZE);

	for (unsigned int i = 0; i < SPIFLASH_PAGE_SIZE; i++)
	{
		_impl->spiflash_write_uint8(buffer[i]);
	}

	_impl->spiflash_end_wait();
}

void SPIFlash::readpages(char *buffer, uint32_t page_number, uint32_t pages)
{
	//printf("SPIFlash::readpages(%p, %u, %u)\n", buffer, page_number, pages);

	_impl->spiflash_read_start(page_number * SPIFLASH_PAGE_SIZE);

	for (unsigned int i = 0; i < SPIFLASH_PAGE_SIZE * pages; i++)
	{
		buffer[i] = _impl->spiflash_read_uint8();
		//printf("READ BYTE %d = %d\n", i, buffer[i]);
	}

	_impl->spiflash_end();
}

int SPIFlash::disk_write(const char *buffer, uint32_t block_number)
{
	//printf("Writing sector %d\n", block_number);

    if (busyflag)
        return 0;

    busyflag = true;

	uint32_t blocks_per_sector = SPIFLASH_SECTOR_SIZE / BLOCK_SIZE;
	uint32_t sectorNumber = block_number / blocks_per_sector;
	uint32_t blockWithinSector = block_number % blocks_per_sector;

	loadsector(sectorNumber);
	memcpy(sectorBuffer + (blockWithinSector * BLOCK_SIZE), buffer, BLOCK_SIZE);
	isSectorWritten = true;

    busyflag = false;

    return 0;
}

void SPIFlash::loadsector(uint32_t sectorNumber)
{
	if (isSectorLoaded && loadedSectorNumber != sectorNumber)
	{
		storesector();
	}

	if (!isSectorLoaded || loadedSectorNumber != sectorNumber)
	{
		uint32_t pages_per_sector = SPIFLASH_SECTOR_SIZE / SPIFLASH_PAGE_SIZE;

		readpages(sectorBuffer, sectorNumber * pages_per_sector, pages_per_sector);

		isSectorLoaded = true;
		loadedSectorNumber = sectorNumber;
	}
}

void SPIFlash::storesector()
{
	if (isSectorLoaded && isSectorWritten)
	{
		uint32_t pages_per_sector = SPIFLASH_SECTOR_SIZE /  SPIFLASH_PAGE_SIZE;

		_impl->spiflash_erase4k(loadedSectorNumber * SPIFLASH_SECTOR_SIZE);

		for (uint32_t i = 0; i < pages_per_sector; i++)
		{
			writepage(sectorBuffer + (i * SPIFLASH_PAGE_SIZE), loadedSectorNumber * pages_per_sector + i);
		}

		isSectorWritten = false;
	}
}

int SPIFlash::disk_read(char *buffer, uint32_t block_number)
{
	//printf("Reading sector %d\n", block_number);

    if (busyflag)
        return 0;

    busyflag = true;

	uint32_t blocks_per_sector = SPIFLASH_SECTOR_SIZE / BLOCK_SIZE;
	uint32_t sectorNumber = block_number / blocks_per_sector;
	uint32_t blockWithinSector = block_number % blocks_per_sector;

	loadsector(sectorNumber);

	memcpy(buffer, sectorBuffer + (blockWithinSector * BLOCK_SIZE), BLOCK_SIZE);

    busyflag = false;

    return 0;
}

int SPIFlash::disk_status() { return 0; }
int SPIFlash::disk_sync() {
    // TODO: wait for DMA, wait for card not busy
    return 0;
}

uint32_t SPIFlash::disk_sectors() { return _sectors / 2; }
uint64_t SPIFlash::disk_size() { return _sectors * SPIFLASH_PAGE_SIZE; }
uint32_t SPIFlash::disk_blocksize() { return SPIFLASH_PAGE_SIZE * 2; }
bool SPIFlash::disk_canDMA() { return false; }

void SPIFlash::periodic() {
	storesector();
}
