// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */

#include "l3/registry.h"
#include "l2/kobject_mount.h"
#include "l2/kobject_process.h"
#include "l2/audit_medusa.h"

struct umount_access {
	MEDUSA_ACCESS_HEADER;
    char mount_path[NAME_MAX + 1];
};

MED_ATTRS(umount_access) {
	MED_ATTR_RO(umount_access, mount_path, "mount_path", MED_STRING),
	MED_ATTR_END
};

MED_ACCTYPE(umount_access, "umount",
            process_kobject, "process",
            mount_kobject, "mount");

static int __init umount_acctype_init(void)
{
	MED_REGISTER_ACCTYPE(umount_access, MEDUSA_ACCTYPE_TRIGGEREDATOBJECT);
	return 0;
}

device_initcall(umount_acctype_init);