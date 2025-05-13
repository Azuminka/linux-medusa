// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */
#include <linux/fs.h>
#include "l3/registry.h"
#include "l2/kobject_mount.h"

struct mount_event {
    MEDUSA_ACCESS_HEADER;
    unsigned long mnt_id;
    const char *path;
};

MED_ATTRS(mount_event) {
    MED_ATTR_RO(mount_event, mnt_id, "id", MED_UNSIGNED),
    MED_ATTR_RO(mount_event, path, "path", MED_STRING),
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
 * mount_kobj_validate_sb - validate and register mount kobject
 * @sb: pointer to superblock representing the mounted filesystem
 *
 * This function validates the mount object represented by the given
 * superblock by initializing its Medusa object and preparing a mount_event
 * structure. It communicates with the authorization server to ensure the
 * object is known and permitted under the current security policy.
 *
 * Return: 1 if validation succeeds, 0 if the authserver is not present,
 *         or MED_ERR on error.
 */
int mount_kobj_validate_sb(struct super_block *sb)
{
	enum medusa_answer_t retval;
	struct mount_event event;
	struct mount_kobject sender;

	if (!med_is_authserver_present())
		return 0;

	init_med_object(&(mount_security(sb)->med_object));

	mount_kern2kobj(&sender, sb);

	event.mnt_id = sender.mnt_id;
	strscpy(event.mount_path, sender.mount_path, sizeof(event.mount_path));

	retval = MED_DECIDE(mount_event, &event, &sender, &sender);
	if (retval != MED_ERR)
		return 1;

	return MED_ERR;
}

device_initcall(mount_evtype_init);