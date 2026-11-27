/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* bell.h - the new-mail bell (bell.c) */
#ifndef BELL_H
#define BELL_H

void bell_init(void);	/* once at the start, before an accessory's first event */
void bell_ring(void);	/* starts the bell and returns at once */

#endif
