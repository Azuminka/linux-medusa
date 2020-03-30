#include <linux/medusa/l3/registry.h>
#include <linux/dcache.h>
#include <linux/limits.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/medusa/l2/audit_medusa.h>

#include "kobject_process.h"
#include "kobject_file.h"
#include <linux/medusa/l1/file_handlers.h>

/* let's define the 'readlink' access type, with subj=task and obj=inode */

struct readlink_access {
	MEDUSA_ACCESS_HEADER;
	char filename[NAME_MAX+1];
};

MED_ATTRS(readlink_access) {
	MED_ATTR_RO (readlink_access, filename, "filename", MED_STRING),
	MED_ATTR_END
};

MED_ACCTYPE(readlink_access, "readlink", process_kobject, "process",
		file_kobject, "file");

int __init readlink_acctype_init(void) {
	MED_REGISTER_ACCTYPE(readlink_access, MEDUSA_ACCTYPE_TRIGGEREDATOBJECT);
	return 0;
}


static medusa_answer_t medusa_do_readlink(struct dentry *dentry);
medusa_answer_t medusa_readlink(struct dentry *dentry)
{
	medusa_answer_t retval = MED_OK;
	struct common_audit_data cad;
	struct medusa_audit_data mad = { .vsi = VSI_UNKNOWN , .event = EVENT_UNKNOWN };

	if (!dentry || IS_ERR(dentry) || dentry->d_inode == NULL)
		return MED_OK;

	cad.type = LSM_AUDIT_DATA_DENTRY;
	cad.u.dentry = dentry;

	if (!MED_MAGIC_VALID(&task_security(current)) &&
		process_kobj_validate_task(current) <= 0)
		goto audit;

	if (!MED_MAGIC_VALID(&inode_security(dentry->d_inode)) &&
			file_kobj_validate_dentry(dentry,NULL) <= 0)
		goto audit;
	
	mad.med_subject = task_security(current)->med_subject;
	mad.med_object = inode_security(dentry->d_inode)->med_object;

	if (!VS_INTERSECT(VSS(&task_security(current)),VS(&inode_security(dentry->d_inode))) ||
		!VS_INTERSECT(VSW(&task_security(current)),VS(&inode_security(dentry->d_inode)))
	) {
		retval = MED_NO;
		mad.vsi = VSI_SW_N;
		goto audit;
	} else
		mad.vsi = VSI_SW;
	if (MEDUSA_MONITORED_ACCESS_O(readlink_access, &inode_security(dentry->d_inode))) {
		retval = medusa_do_readlink(dentry);
		mad.event = EVENT_MONITORED;
	} else
		mad.event = EVENT_MONITORED_N;
	
audit:
#ifdef CONFIG_AUDIT
	mad.function = __func__;
	mad.med_answer = retval;
	cad.medusa_audit_data = &mad;
	medusa_audit_log_callback(&cad);
#endif
	return retval;	
}

/* XXX Don't try to inline this. GCC tries to be too smart about stack. */
static medusa_answer_t medusa_do_readlink(struct dentry *dentry)
{
	struct readlink_access access;
	struct process_kobject process;
	struct file_kobject file;
	medusa_answer_t retval;

        memset(&access, '\0', sizeof(struct readlink_access));
        /* process_kobject process is zeroed by process_kern2kobj function */
        /* file_kobject file is zeroed by file_kern2kobj function */
    
	file_kobj_dentry2string(dentry, access.filename);
	
    process_kern2kobj(&process, current);
	
    file_kern2kobj(&file, dentry->d_inode);
	
    file_kobj_live_add(dentry->d_inode);
	retval = MED_DECIDE(readlink_access, &access, &process, &file);
	file_kobj_live_remove(dentry->d_inode);
	
    if (retval != MED_ERR)
		return retval;
	return MED_OK;
}
__initcall(readlink_acctype_init);
