// SPDX-License-Identifier: GPL-2.0
#include <linux/dcache.h>
#include <linux/fs.h> 
#include <linux/mount.h> 
#include <linux/string.h>

#include "l3/registry.h"
#include "l2/kobject_mount.h"

inline int mount_kern2kobj(struct mount_kobject *mk, const struct path *path)
{
    return 0;
}

static inline int mount_kobj2kern(struct mount_kobject *mk, const struct path *path)
{
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
    mount_upadte,
    NULL                     /* unmonitor */
};

static int __init mount_kobject_init(void)
{
	MED_REGISTER_KCLASS(mount_kobject);
	return 0;
}

device_initcall(mount_kobject_init);