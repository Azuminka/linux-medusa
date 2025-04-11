// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) 2025 by Stanislava Peckova
 */

#ifndef __MEDUSA_L1_MOUNT_H__
#define __MEDUSA_L1_MOUNT_H__

#include <linux/path.h>
#include "l3/med_model.h"

/* prototypes of L2 process related handlers called from L1 hooks */
extern enum medusa_answer_t medusa_mount(const char *dev_name, const struct path *path, const char *type, unsigned long flags, void *data);
extern enum medusa_answer_t medusa_umount(const struct path *path);
extern enum medusa_answer_t medusa_remount(const struct path *path, unsigned long flags, void *data);
extern enum medusa_answer_t medusa_pivotroot(const struct path *old_path, const struct path *new_path);
extern enum medusa_answer_t medusa_move_mount(const struct path *from_path, const struct path *to_path);

extern struct lsm_blob_sizes medusa_blob_sizes;

/**
 * mount_security - access Medusa L1 mount security data from a given path
 * @path: pointer to struct path which represents the mount point
 *
 * This macro computes a pointer to the Medusa L1 mount security structure
 * stored in the s_security blob of the superblock associated with the given
 * path. It assumes that the superblock security blob has been allocated and
 * initialized by the LSM core.
 *
 * Return: pointer to struct medusa_l1_mount_s associated with the given path
 */
#define mount_security(path) \
	((struct medusa_l1_mount_s *)((char *)(path)->mnt->mnt_sb->s_security + medusa_blob_sizes.lbs_superblock))

/**
 * struct medusa_l1_mount_s - Medusa L1 security data for superblock objects
 * @mount_class: classification of the mounted filesystem (currently unused)
 * @med_object: subject identity structure containing metadata of the task
 * that performed the mount
 *
 * This structure is used by Medusa to store security metadata associated with
 * mounted filesystems (superblocks). It is embedded into the LSM security blob
 * (s_security) of each super_block and is initialized by the sb_alloc_security
 * hook.
 */
struct medusa_l1_mount_s {
	unsigned int mount_class;
	struct medusa_object_s med_object;
};

#endif /* __MEDUSA_L1_MOUNT_H__ */