/* SPDX-License-Identifier: GPL-2.0 */
/* process_kobject.h, (C) 2002 Milan Pikula */

#ifndef _TASK_KOBJECT_H
#define _TASK_KOBJECT_H

/* TASK kobject: this file defines the kobject structure for task, e.g.
 * the data, which we want to pass to the authorization server.
 *
 * The structure contains some data from ordinary task_struct
 * (such as pid etc.), and some data from medusa_l1_task_s, which is
 * defined in medusa/l1/task.h.
 */

#include <linux/sched.h>	/* contains all includes we need ;) */
#include <linux/medusa/l3/kobject.h>
#include <linux/medusa/l1/task.h>
#include <linux/medusa/l1/process_handlers.h>

struct process_kobject {
	int pid, pgrp, tgid, session;
	int parent_pid, child_pid, sibling_pid;
	unsigned int uid, euid, suid, fsuid;
	unsigned int gid, egid, sgid, fsgid;
#ifdef CONFIG_AUDIT
	unsigned int luid;
#endif
	char cmdline[128];

	kernel_cap_t ecap, icap, pcap, acap, bcap;
	struct medusa_object_s med_object;
	struct medusa_subject_s med_subject;
#if (defined(CONFIG_X86) || defined(CONFIG_X86_64)) && defined(CONFIG_MEDUSA_SYSCALL)
	/* bitmap of syscalls, which are reported; only on x86 arch */
	unsigned char med_syscall[NR_syscalls / (sizeof(unsigned char) * 8)];
#endif
};
extern MED_DECLARE_KCLASSOF(process_kobject);

int process_kern2kobj(struct process_kobject *tk, struct task_struct *ts);

#endif
