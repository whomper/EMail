/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * migrate_eml.h - the .EML files of EMail 1.41 and before moved into
 * the folder's pack, once (migrate_eml.c). Temporary: see there.
 */
#ifndef MIGRATE_EML_H
#define MIGRATE_EML_H

#include "store.h"

void migrate_eml(FOLDER *f);

#endif
