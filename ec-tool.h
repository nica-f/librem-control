/*
 * Copyright (C) 2021 Nicole Faerber <nicole.faerber@puri.sm>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

int port_open(void);

int port_read(int fd, off_t offset, size_t len, void *buf);

int port_write(int fd, off_t offset, size_t len, void *buf);

int cmd_read(int fd);

int cmd_write(int fd, u_int8_t cmd);

int cmd_result(int fd);

int cmd_data_read(int fd, int len, void *buf);

int cmd_data_write(int fd, u_int8_t cmd, void *cmd_data, int len);

void spi_read(int fd);

int get_ec_board(int fd, void *buf);

int get_ec_version(int fd, void *buf);

