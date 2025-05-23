/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */

#ifndef _MOUNT_KOBJECT_H
#define _MOUNT_KOBJECT_H

#include "l3/kobject.h"
#include "l1/mount.h"

struct mount_kobject {
	unsigned long 			mnt_id;
    char                    mount_path[PATH_MAX];
    struct medusa_object_s 	med_object;
};

extern MED_DECLARE_KCLASSOF(mount_kobject);

int mount_kern2kobj(struct mount_kobject *mk, const struct path *path);
int resolve_mount_path_by_id(unsigned long mnt_id, struct path *out_path);
#endif