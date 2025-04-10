/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _MOUNT_KOBJECT_H
#define _MOUNT_KOBJECT_H

#include "l3/kobject.h"
#include "l1/mount.h"

struct mount_kobject {
	unsigned long 			mnt_id;
    const char 				*path;
    struct medusa_object_s 	med_object;
};

extern MED_DECLARE_KCLASSOF(mount_kobject);

#endif