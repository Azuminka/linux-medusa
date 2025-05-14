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
	MED_ATTR_RO(mount_kobject, mount_path, "path", MED_STRING),
	MED_ATTR_OBJECT(mount_kobject),
	MED_ATTR_END
};

/**
 * mount_kern2kobj - fill mount_kobject structure from kernel path
 * @mk: pointer to a mount_kobject structure
 * @path: pointer to the kernel path structure
 *
 * This function fills the provided mount_kobject structure with relevant data
 * extracted from a kernel path. It sets the mount ID, copies the string
 * representation of the mount point path into the fixed-size buffer
 * mount_path, and copies the subject identity (med_object) from the Medusa
 * L1 security blob associated with the superblock.
 *
 * Return: 0 on success,
 *         -EINVAL if input pointers are invalid,
 *         -ENOMEM if memory allocation fails,
 *         other negative error code if path formatting fails.
 */
inline int mount_kern2kobj(struct mount_kobject *mk, const struct path *path)
{
    char *resolved_path;

    if (unlikely(!mk || !path || !path->mnt || !path->dentry || !path->mnt->mnt_sb || !mount_security(path->mnt->mnt_sb))) {
		med_pr_err("ERROR: NULL pointer: %s: mk=%p or path or security blob is NULL",
		           __func__, mk);
		return -EINVAL;
	}

    memset(mk, 0, sizeof(struct mount_kobject));

    mk->mnt_id = real_mount(path->mnt)->mnt_id;

	resolved_path = dentry_path_raw(path->dentry, mk->mount_path, sizeof(mk->mount_path));
	if (IS_ERR(resolved_path)) {
		med_pr_err("ERROR: Failed to get mount path in %s: %ld", __func__, PTR_ERR(resolved_path));
		return PTR_ERR(resolved_path);
	}

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

/**
 * resolve_mount_path_by_id - resolve struct path from mount ID
 * @mnt_id: mount identifier to look up
 * @out_path: pointer to struct path to be filled on success
 *
 * Return: 0 on success, -ENOENT if no matching mount is found
 */
int resolve_mount_path_by_id(unsigned long mnt_id, struct path *out_path)
{
	struct mnt_namespace *ns;
	struct mount *mnt;
	int ret = -ENOENT;

	rcu_read_lock();

	ns = rcu_dereference(current->nsproxy->mnt_ns);
	if (!ns)
		goto out;

	mnt = ns->root;

	if (mnt->mnt_id == mnt_id) {
		out_path->mnt = &mnt->mnt;
		out_path->dentry = mnt->mnt.mnt_root;
		path_get(out_path);
		ret = 0;
	}

out:
	rcu_read_unlock();
	return ret;
}

/**
 * mount_fetch - refetch mount_kobject data from the kernel
 * @kobj: pointer to a generic Medusa kernel object
 *
 * This function attempts to refetch up-to-date information about a mounted
 * filesystem based on the mount ID stored in the provided mount_kobject
 * structure. It locates the corresponding kernel path and fills the object
 * with the latest data from the VFS layer and Medusa L1 security blob.
 *
 * Return: pointer to the updated medusa_kobject_s on success,
 *         NULL if refetching or reconstruction failed.
 */
static struct medusa_kobject_s *mount_fetch(struct medusa_kobject_s *kobj)
{
	struct mount_kobject *mk;
	struct path resolved_path;
	struct medusa_kobject_s *retval = NULL;

	mk = (struct mount_kobject *)kobj;
	if (!mk)
		goto out_err;

	if (resolve_mount_path_by_id(mk->mnt_id, &resolved_path) != 0)
		goto out_err;

	retval = kobj;
	if (unlikely(mount_kern2kobj(mk, &resolved_path) < 0))
		retval = NULL;

	path_put(&resolved_path);

out_err:
	return retval;
}

/**
 * Currently not needed: no runtime updates to mount_kobject are required.
 */
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