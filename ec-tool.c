/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2022 Nicole Faerber <nicole.faerber@puri.sm>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether expressed or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>


#define ACPI_PATH_1 "/sys/bus/acpi/devices/316D4C14:00"
#define ACPI_PATH_2 "/sys/bus/acpi/devices/PURI4543:00"

#define SMFI_CMD_BASE 0xE00
#define SMFI_CMD_SIZE 0x100

#define SMFI_DBG_BASE 0xF00
#define SMFI_DBG_SIZE 0x100

#define SMFI_CMD_CMD 0x00
#define SMFI_CMD_RES 0x01
#define SMFI_CMD_DATA 0x02

#define CMD_SPI_FLAG_READ	(1 << 0)
#define CMD_SPI_FLAG_DISABLE	(1 << 1)
#define CMD_SPI_FLAG_SCRATCH	(1 << 2)
#define CMD_SPI_FLAG_BACKUP	(1 << 3)

// reset = flags, read false, disable true

enum Command {
    // Indicates that EC is ready to accept commands
    CMD_NONE = 0,
    // Probe for System76 EC protocol
    CMD_PROBE = 1,
    // Read board string
    CMD_BOARD = 2,
    // Read version string
    CMD_VERSION = 3,
    // Write bytes to console
    CMD_PRINT = 4,
    // Access SPI chip
    CMD_SPI = 5,
    // Reset EC
    CMD_RESET = 6,
    // Get fan speeds
    CMD_FAN_GET = 7,
    // Set fan speeds
    CMD_FAN_SET = 8,
    // Get keyboard map index
    CMD_KEYMAP_GET = 9,
    // Set keyboard map index
    CMD_KEYMAP_SET = 10,
    // Get LED value by index
    CMD_LED_GET_VALUE = 11,
    // Set LED value by index
    CMD_LED_SET_VALUE = 12,
    // Get LED color by index
    CMD_LED_GET_COLOR = 13,
    // Set LED color by index
    CMD_LED_SET_COLOR = 14,
    // Get LED matrix mode and speed
    CMD_LED_GET_MODE = 15,
    // Set LED matrix mode and speed
    CMD_LED_SET_MODE = 16,
    // Get key matrix state
    CMD_MATRIX_GET = 17,
    // Save LED settings to ROM
    CMD_LED_SAVE = 18,
    //TODO
};

enum Result {
    // Command executed successfully
    RES_OK = 0,
    // Command failed with generic error
    RES_ERR = 1,
    //TODO
};

#if 0
enum CommandSpiFlag {
    // Read from SPI chip if set, write otherwise
    CMD_SPI_FLAG_READ = (1 << 0),
    // Disable SPI chip after executing command
    CMD_SPI_FLAG_DISABLE = (1 << 1),
    // Run firmware from scratch RAM if necessary
    CMD_SPI_FLAG_SCRATCH = (1 << 2),
    // Write to backup ROM instead
    CMD_SPI_FLAG_BACKUP = (1 << 3),
};
#endif


int port_open(void)
{
int fd;
struct stat path_stat;

    if (getuid() != 0 && geteuid() != 0) {
        fprintf(stderr, "please run as root or use sudo or similar\n");
        return -1;
    }
    if (stat(ACPI_PATH_1, &path_stat) != 0 &&
        stat(ACPI_PATH_2, &path_stat) != 0) {
        fprintf(stderr, "no Librem EC found, giving up\n");
        return -1;
    }

    if (S_ISDIR(path_stat.st_mode))
        fprintf(stderr, "Librem EC detected\n");

    fd = open("/dev/port", O_RDWR);
    if (fd<0)
        perror("open()");

    return fd;
}

int port_read(int fd, off_t offset, size_t len, void *buf)
{
int rlen=0;

    lseek(fd, offset, SEEK_SET);
    rlen = read(fd, buf, len);

    return rlen;
}

int port_write(int fd, off_t offset, size_t len, void *buf)
{
int wlen=0;

    lseek(fd, offset, SEEK_SET);
    wlen = write(fd, buf, len);

    return wlen;
}

int cmd_read(int fd)
{
unsigned char buf=0;

    if (port_read(fd, SMFI_CMD_BASE + SMFI_CMD_CMD, 1, &buf) != 1)
        return -1;
    else
        return buf;
}

int cmd_write(int fd, u_int8_t cmd)
{
int i;

    i = port_write(fd, SMFI_CMD_BASE + SMFI_CMD_CMD, 1, &cmd);
    if (i < 1)
        return -1;

    i=100;
    while (--i > 0) {
        if (cmd_read(fd) == 0)
            break;
        usleep(100);
    }
    return (i>0 ? 1:0);
}

int cmd_result(int fd)
{
    return -1;
}

int cmd_data_read(int fd, int len, void *buf)
{
    if (buf == NULL)
        return -ENOBUFS;

    if (len > (SMFI_CMD_SIZE - SMFI_CMD_DATA))
        len = (SMFI_CMD_SIZE - SMFI_CMD_DATA);

    return port_read(fd, SMFI_CMD_BASE + SMFI_CMD_DATA, len, buf);
}

int cmd_data_write(int fd, u_int8_t cmd, void *cmd_data, int len)
{
    port_write(fd, SMFI_CMD_BASE + SMFI_CMD_DATA, len, cmd_data);

    cmd_write(fd, cmd);

    return -1;
}


void spi_read(int fd)
{
    unsigned char buf[0x100] = { CMD_SPI_FLAG_READ | CMD_SPI_FLAG_SCRATCH | CMD_SPI_FLAG_BACKUP, 0x00, 0x00, 0x00, 0x00 };

    cmd_data_write(fd, CMD_SPI, buf, 5);

    cmd_data_read(fd, 0x60, buf);

    fprintf(stderr, "SPI first 0x60:\n");
    for (int i=0; i<0x60; i++) {
        if ((buf[i]) >31 && buf[i]<128)
            fprintf(stderr, "%c", buf[i]);
        else
            fprintf(stderr, ".");
    }
    fprintf(stderr, "\n");
}


int get_ec_board(int fd, void *buf)
{
    if (cmd_write(fd, CMD_BOARD) != 1) {
        fprintf(stderr, "cmd fail\n");
        close(fd);
        return -1;
    }
    if (cmd_data_read(fd, 0x100-2, buf) <= 0) {
        fprintf(stderr, "data fail\n");
        close(fd);
        return -1;
    }
    return 0;
}

int get_ec_version(int fd, void *buf)
{
    if (cmd_write(fd, CMD_VERSION) != 1) {
        fprintf(stderr, "cmd fail\n");
        close(fd);
        return -1;
    }
    if (cmd_data_read(fd, 0x100-2, buf) <= 0) {
        fprintf(stderr, "data fail\n");
        close(fd);
        return -1;
    }
    return 0;
}

#if 0
    /// Read at a specific address
    pub unsafe fn read_at(&mut self, address: u32, data: &mut [u8]) -> Result<usize, Error> {
        if (address & 0xFF00_0000) > 0 {
            return Err(Error::Parameter);
        }

        self.spi.reset()?;
        self.spi.write(&[
            0x0B,
            (address >> 16) as u8,
            (address >> 8) as u8,
            address as u8,
            0,
        ])?;
        self.spi.read(data)
    }
reset:
CMD_SPI (data[0]=flags(false,true), data[1]=0
read:
CMD_SPI (data[0]=flags(true, false), data[1]=len
write:
CMD_SPI
  [0]flags false,false
  [1]len
  data[len]
#endif

#if 0
int main(int argc, char **argv)
{
int fd;
unsigned char buf[0x100];

    fd=port_open();
    if (fd < 0)
        return -1;

    if (get_ec_board(fd, buf))
        goto out;
    fprintf(stderr, "Board  : %s\n", buf);

    if (get_ec_version(fd, buf))
        goto out;
    fprintf(stderr, "Version: %s\n", buf);

    // spi_read(fd);

out:
    close(fd);

    return 0;
}
#endif
