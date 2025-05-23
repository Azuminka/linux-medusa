// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */
#include "../../../../fs/mount.h"
#include <linux/fs.h>
#include "l3/registry.h"
#include "l2/kobject_mount.h"

struct mount_event {
    MEDUSA_ACCESS_HEADER;
};

MED_ATTRS(mount_event) {
    MED_ATTR_END
};

MED_EVTYPE(mount_event, "getmount", mount_kobject, "mount", mount_kobject, "mount");

static int __init mount_evtype_init(void)
{
	MED_REGISTER_EVTYPE(mount_event,
			    MEDUSA_EVTYPE_TRIGGEREDATSUBJECT |
			    MEDUSA_EVTYPE_TRIGGEREDBYOBJECTBIT |
			    MEDUSA_EVTYPE_NOTTRIGGERED);
	return 0;
}

/**
 * mount_kobj_validate_path - validate and register mount kobject from path
 * @path: pointer to the kernel path structure
 *
 * This function resolves a mount_kobject from the given path and sends a
 * validation request to the authorization server using the mount_event.
 *
 * Return: 1 on success,
 *         0 on authserver absence or error,
 *         MED_ERR if the validation was denied.
 */
int mount_kobj_validate_path(const struct path *path)
{
	enum medusa_answer_t retval;
	struct mount_event event;
	struct mount_kobject *sender;

	if (!med_is_authserver_present())
		return 0;

	sender = (struct mount_kobject*)kmalloc(sizeof(struct mount_kobject), GFP_KERNEL);
	if (!sender)
		return MED_ERR;
	
	init_med_object(&(mount_security(path->mnt->mnt_sb)->med_object));
	if (mount_kern2kobj(sender, path) < 0) {
		kfree(sender);
		return MED_ERR;
	}

	retval = MED_DECIDE(mount_event, &event, sender, sender);
	kfree(sender);
	if (retval != MED_ERR)
		return 1;

	return MED_ERR;
}

device_initcall(mount_evtype_init);