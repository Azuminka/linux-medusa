// SPDX-License-Identifier: GPL-2.0-only

#include "l3/registry.h"
#include "l2/kobject_process.h"
#include "l2/kobject_file.h"
#include "l2/audit_medusa.h"

/* let's define the 'mknod' access type, with subj=task and obj=inode */

struct mknod_access {
	MEDUSA_ACCESS_HEADER;
	char filename[NAME_MAX + 1];
	dev_t dev;
	int mode;
};

MED_ATTRS(mknod_access) {
	MED_ATTR_RO(mknod_access, filename, "filename", MED_STRING),
	MED_ATTR_RO(mknod_access, dev, "dev", MED_UNSIGNED),
	MED_ATTR_RO(mknod_access, mode, "mode", MED_UNSIGNED),
	MED_ATTR_END
};

MED_ACCTYPE(mknod_access, "mknod",
	    process_kobject, "process",
	    file_kobject, "file");

int __init mknod_acctype_init(void)
{
	MED_REGISTER_ACCTYPE(mknod_access, MEDUSA_ACCTYPE_TRIGGEREDATOBJECT);
	return 0;
}

static void medusa_mknod_pacb(struct audit_buffer *ab, void *pcad)
{
	struct common_audit_data *cad = pcad;
	struct medusa_audit_data *mad = cad->medusa_audit_data;

	audit_log_d_path(ab, " dir=", mad->path);
	audit_log_format(ab, " name=");
	spin_lock(&mad->dentry->d_lock);
	audit_log_untrustedstring(ab, mad->dentry->d_name.name);
	spin_unlock(&mad->dentry->d_lock);
	audit_log_format(ab, " mode=%d", mad->pacb.mknod.mode);
	audit_log_format(ab, " dev=%02x:%02x", MAJOR(mad->pacb.mknod.dev),
			 MINOR(mad->pacb.mknod.dev));
}

/* XXX Don't try to inline this. GCC tries to be too smart about stack. */
static enum medusa_answer_t medusa_do_mknod(const struct path *dir,
					    struct dentry *dentry,
					    int mode,
					    dev_t dev)
{
	struct mknod_access access;
	struct process_kobject process;
	struct file_kobject file;
	enum medusa_answer_t retval;

	file_kobj_dentry2string_mnt(dir, dentry, access.filename);
	access.dev = dev;
	access.mode = mode;
	process_kern2kobj(&process, current);
	file_kern2kobj(&file, dir->dentry->d_inode);
	file_kobj_live_add(dir->dentry->d_inode);
	retval = MED_DECIDE(mknod_access, &access, &process, &file);
	file_kobj_live_remove(dir->dentry->d_inode);
	return retval;
}

enum medusa_answer_t medusa_mknod(const struct path *dir,
				  struct dentry *dentry,
				  umode_t mode,
				  unsigned int dev)
{
	struct path ndcurrent, ndupper;
	enum medusa_answer_t retval = MED_ALLOW;
	struct common_audit_data cad;
	struct medusa_audit_data mad = { .vsi = VS_SW_N };

	if (!is_med_magic_valid(&(task_security(current)->med_object)) &&
	    process_kobj_validate_task(current) <= 0) {
		retval = MED_ALLOW;
		goto audit;
	}

	ndcurrent = *dir;
	medusa_get_upper_and_parent(&ndcurrent, &ndupper, NULL);

	if (!is_med_magic_valid(&(inode_security(ndupper.dentry->d_inode)->med_object)) &&
	    file_kobj_validate_dentry(ndupper.dentry, ndupper.mnt, NULL) <= 0) {
		medusa_put_upper_and_parent(&ndupper, NULL);
		retval = MED_ALLOW;
		goto audit;
	}
	if (!vs_intersects(VSS(task_security(current)),
			   VS(inode_security(ndupper.dentry->d_inode))) ||
	    !vs_intersects(VSW(task_security(current)),
			   VS(inode_security(ndupper.dentry->d_inode)))) {
		mad.vs.sw.vst = VS(inode_security(ndupper.dentry->d_inode));
		mad.vs.sw.vss = VSS(task_security(current));
		mad.vs.sw.vsw = VSW(task_security(current));
		medusa_put_upper_and_parent(&ndupper, NULL);
		retval = MED_DENY;
		goto audit;
	} else {
		mad.vsi = VS_INTERSECT;
	}
	if (MEDUSA_MONITORED_ACCESS_O(mknod_access, inode_security(ndupper.dentry->d_inode))) {
		retval = medusa_do_mknod(&ndupper, dentry, mode, dev);
		mad.event = EVENT_MONITORED;
	} else {
		retval = MED_ALLOW;
		mad.event = EVENT_MONITORED_N;
	}
	medusa_put_upper_and_parent(&ndupper, NULL);
audit:
#ifdef CONFIG_AUDIT
	if (task_security(current)->audit) {
		cad.type = LSM_AUDIT_DATA_TASK;
		cad.u.tsk = current;
		mad.function = "mknod";
		mad.med_answer = retval;
		mad.path = dir;
		mad.dentry = dentry;
		/* if hook is changed from inode to path this needs to be changed by calling
		 * new_decode_dev(dev)
		 */
		mad.pacb.mknod.dev = dev;
		mad.pacb.mknod.mode = mode;
		cad.medusa_audit_data = &mad;
		medusa_audit_log_callback(&cad, medusa_mknod_pacb);
	}
#endif
	return retval;
}

device_initcall(mknod_acctype_init);
