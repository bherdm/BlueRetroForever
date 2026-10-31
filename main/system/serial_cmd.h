/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SERIAL_CMD_H_
#define _SERIAL_CMD_H_

/* Read and run the command lines waiting on the console UART. Call from the system manager task only. */
void serial_cmd_poll(void);

#endif /* _SERIAL_CMD_H_ */
