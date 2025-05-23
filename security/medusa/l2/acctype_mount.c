// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */

#include "l3/registry.h"
#include "l2/kobject_mount.h"
#include "l2/kobject_process.h"
#include "l2/audit_medusa.h"

struct mount_access {
	MEDUSA_ACCESS_HEADER;
};

MED_ATTRS(mount_access) {
	MED_ATTR_END
};

MED_ACCTYPE(mount_access, "mount",
            process_kobject, "process",
            mount_kobject, "mnt");

static int __init mount_acctype_init(void)
{
	MED_REGISTER_ACCTYPE(mount_access, MEDUSA_ACCTYPE_TRIGGEREDATOBJECT);
	return 0;
}

/**
 * medusa_do_mount - Perform Medusa authorization for mount operation
 * @path: target mount point
 *
 * Converts kernel data to Medusa kobjects and performs access check for mount.
 * Allocates and fills mount_kobject, prepares process object, and sends 
 * request to authorization server using the mount_access rule.
 *
 * Return: decision from authorization server (MED_ALLOW, MED_DENY, or MED_ERR)
 */
static enum medusa_answer_t medusa_do_mount(const struct path *path)
{
	struct mount_access access;
	struct process_kobject process;
	struct mount_kobject *mount;
	enum medusa_answer_t retval;

	mount = (struct mount_kobject*)kmalloc(sizeof(struct mount_kobject), GFP_KERNEL);
	if(!mount)
		return MED_ERR;
	
	if (mount_kern2kobj(mount, path) < 0)
		return MED_ERR;

	process_kern2kobj(&process, current);

	retval = MED_DECIDE(mount_access, &access, &process, mount);
	kfree(mount);
	return retval;
}

/**
 * medusa_mount - Hook for mount operation in Medusa LSM
 * @dev_name: device name to mount (may be NULL)
 * @path: target mount point
 * @type: filesystem type (may be NULL)
 * @flags: mount flags
 * @data: filesystem-specific data (may be NULL)
 *
 * Validates the current task and mount target. If the operation is monitored,
 * it is authorized via medusa_do_mount() and optionally audited.
 *
 * Return: MED_ALLOW if permitted,
 *         MED_DENY or MED_ERR otherwise.
 */
enum medusa_answer_t medusa_mount(const char *dev_name, const struct path *path, const char *type, unsigned long flags, void *data)
{
	struct common_audit_data cad;
	struct medusa_audit_data mad = {
		.ans = MED_ALLOW,
		.as = AS_NO_REQUEST,
	};

	if (!is_med_magic_valid(&task_security(current)->med_object) &&
	    process_kobj_validate_task(current) <= 0)
		return mad.ans;

	if (!path || !path->mnt || !path->mnt->mnt_sb ||
	    !is_med_magic_valid(&mount_security(path->mnt->mnt_sb)->med_object)) {
		if (mount_kobj_validate_path(path) <= 0)
			return mad.ans;
	}

	if (MEDUSA_MONITORED_ACCESS_O(mount_access, mount_security(path->mnt->mnt_sb))) {
		mad.ans = medusa_do_mount(path);
		mad.as = AS_REQUEST;
	}

	if (task_security(current)->audit) {
		cad.type = LSM_AUDIT_DATA_PATH;
		cad.u.path = *path;
		mad.function = "mount";
		mad.path.path = path;
		mad.path.mode = 0;
		cad.medusa_audit_data = &mad;
		medusa_audit_log_callback(&cad, medusa_path_mode_cb);
	}

	return mad.ans;
}


device_initcall(mount_acctype_init);