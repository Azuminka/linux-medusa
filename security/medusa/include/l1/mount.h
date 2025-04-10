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

struct medusa_l1_mount_s {
	unsigned int mount_class;
	struct medusa_object_s med_object;
};

#endif /* __MEDUSA_L1_MOUNT_H__ */