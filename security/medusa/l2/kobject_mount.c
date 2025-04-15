// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */

#include <linux/string.h>

#include "../../../../fs/mount.h"
#include "l3/registry.h"
#include "l2/kobject_mount.h"

MED_ATTRS(mount_kobject) {
	MED_ATTR_KEY_RO(mount_kobject, mnt_id, "mnt_id", MED_UNSIGNED),
	MED_ATTR_RO(mount_kobject, path, "path", MED_STRING),
	MED_ATTR_OBJECT(mount_kobject),
	MED_ATTR_END
};

/**
 * mount_kern2kobj - fill mount_kobject structure from kernel path
 * @mk: pointer to a mount_kobject structure
 * @path: pointer to the kernel path structure
 *
 * This function fills the provided mount_kobject structure with relevant data
 * extracted from a kernel path. It copies the mount ID, allocates and stores a
 * string representation of the mount point path and copies the subject identity
 * (med_object) from the Medusa L1 security blob associated with the superblock.
 *
 * Return: 0 on success,
 *         -EINVAL if input pointers are invalid,
 *         -ENOMEM if memory allocation fails,
 *         other negative error code if path formatting fails.
 */
inline int mount_kern2kobj(struct mount_kobject *mk, const struct path *path)
{
    char *buf;

    if (unlikely(!mk || !path || !path->mnt || !path->dentry || !path->mnt->mnt_sb || !mount_security(path->mnt->mnt_sb))) {
		med_pr_err("ERROR: NULL pointer: %s: mk=%p or path or security blob is NULL",
		           __func__, mk);
		return -EINVAL;
	}

    memset(mk, 0, sizeof(struct mount_kobject));

    mk->mnt_id = real_mount(path->mnt)->mnt_id;

    buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mk->path = kstrdup(dentry_path_raw(path->dentry, buf, PATH_MAX), GFP_KERNEL);
	kfree(buf);

	if (IS_ERR(mk->path))
		return PTR_ERR(mk->path);
	if (!mk->path)
		return -ENOMEM;

	mk->med_object = mount_security(path->mnt->mnt_sb)->med_object;

    return 0;
}

/**
 * mount_kobj2kern - write mount_kobject data back to kernel structure
 * @mk: pointer to Medusa mount_kobject structure
 * @sb: pointer to the kernel super_block structure representing the mounted fs
 *
 * This function writes data from the given mount_kobject back into kernel's L1
 * security blob associated with the specified superblock.
 *
 * Return: 0 on success,
 *         -EINVAL if any pointer is NULL or the security blob is missing
 */
static inline int mount_kobj2kern(struct mount_kobject *mk, struct super_block *sb)
{
	if (unlikely(!mk || !sb || !mount_security(sb))) {
		med_pr_err("ERROR: NULL pointer in %s: mk=%p sb=%p sec=%p",
		           __func__, mk, sb, mount_security(sb));
		return -EINVAL;
	}

	mount_security(sb)->med_object = mk->med_object;
	med_magic_validate(&mount_security(sb)->med_object);

	return 0;
}

static struct medusa_kobject_s *mount_fetch(struct medusa_kobject_s *kobj)
{
    return NULL;
}

static enum medusa_answer_t mount_update(struct medusa_kobject_s *kobj)
{
    enum medusa_answer_t retval = MED_ERR;
    return retval;
}

MED_KCLASS(mount_kobject) {
    MEDUSA_KCLASS_HEADER(mount_kobject),
    "mount",
    NULL,                    /* init */
    NULL,                    /* destroy */
    mount_fetch,
    mount_update,
    NULL                     /* unmonitor */
};

static int __init mount_kobject_init(void)
{
	MED_REGISTER_KCLASS(mount_kobject);
	return 0;
}

device_initcall(mount_kobject_init);