// SPDX-License-Identifier: GPL-2.0

#include "l3/registry.h"
#include "l2/kobject_process.h"
#include "l2/kobject_file.h"
#include "l2/audit_medusa.h"

/* let's define the 'create' access type, with subj=task and obj=inode */

struct create_access {
	MEDUSA_ACCESS_HEADER;
	char filename[NAME_MAX + 1];
	int mode;
};

MED_ATTRS(create_access) {
	MED_ATTR_RO(create_access, filename, "filename", MED_STRING),
	MED_ATTR_RO(create_access, mode, "mode", MED_UNSIGNED),
	MED_ATTR_END
};

MED_ACCTYPE(create_access, "create",
	    process_kobject, "process",
	    file_kobject, "file");

static int __init create_acctype_init(void)
{
	MED_REGISTER_ACCTYPE(create_access, MEDUSA_ACCTYPE_TRIGGEREDATOBJECT);
	return 0;
}

/* XXX Don't try to inline this. GCC tries to be too smart about stack. */
static enum medusa_answer_t medusa_do_create(struct dentry *parent, struct dentry *dentry, int mode)
{
	struct create_access access;
	struct process_kobject process;
	struct file_kobject file;
	enum medusa_answer_t retval;

	file_kobj_dentry2string(dentry, access.filename);
	access.mode = mode;
	process_kern2kobj(&process, current);
	file_kern2kobj(&file, parent->d_inode);
	file_kobj_live_add(parent->d_inode);
	retval = MED_DECIDE(create_access, &access, &process, &file);
	if (retval == MED_ERR)
		retval = MED_ALLOW;
	file_kobj_live_remove(parent->d_inode);
	return retval;
}

static void medusa_create_pacb(struct audit_buffer *ab, void *pcad)
{
	struct common_audit_data *cad = pcad;
	struct medusa_audit_data *mad = cad->medusa_audit_data;

	audit_log_format(ab, " mode=%d", mad->path.mode);
}

enum medusa_answer_t medusa_create(struct dentry *dentry, int mode)
{
	struct path ndcurrent, ndupper, ndparent;
	struct common_audit_data cad;
	struct medusa_audit_data mad = { .ans = MED_ALLOW, .as = AS_NO_REQUEST };

	if (!is_med_magic_valid(&(task_security(current)->med_object)) &&
	    process_kobj_validate_task(current) <= 0)
		return mad.ans;

	ndcurrent.dentry = dentry;
	ndcurrent.mnt = NULL;
	medusa_get_upper_and_parent(&ndcurrent, &ndupper, &ndparent);

	if (!is_med_magic_valid(&(inode_security(ndparent.dentry->d_inode)->med_object)) &&
	    file_kobj_validate_dentry(ndparent.dentry, ndparent.mnt, NULL) <= 0) {
		medusa_put_upper_and_parent(&ndupper, &ndparent);
		return mad.ans;
	}
	if (!vs_intersects(VSS(task_security(current)),
			   VS(inode_security(ndparent.dentry->d_inode))) ||
	    !vs_intersects(VSW(task_security(current)),
			   VS(inode_security(ndparent.dentry->d_inode)))) {
		mad.vs.sw.vst = VS(inode_security(ndparent.dentry->d_inode));
		mad.vs.sw.vss = VSS(task_security(current));
		mad.vs.sw.vsw = VSW(task_security(current));
		medusa_put_upper_and_parent(&ndupper, &ndparent);
		mad.ans = MED_DENY;
		goto audit;
	}
	if (MEDUSA_MONITORED_ACCESS_O(create_access, inode_security(ndparent.dentry->d_inode))) {
		mad.ans = medusa_do_create(ndparent.dentry, ndupper.dentry, mode);
		mad.as = AS_REQUEST;
	}
	medusa_put_upper_and_parent(&ndupper, &ndparent);
audit:
	/* TODO: This should be LSM_AUDIT_DATA_NONE, but acctype_create is not
	 * used anyway. */
	cad.type = LSM_AUDIT_DATA_DENTRY;
	cad.u.dentry = dentry;
	mad.function = "create";
	mad.path.mode = mode;
	cad.medusa_audit_data = &mad;
	medusa_audit_log_callback(&cad, medusa_create_pacb);
	return mad.ans;
}

device_initcall(create_acctype_init);
