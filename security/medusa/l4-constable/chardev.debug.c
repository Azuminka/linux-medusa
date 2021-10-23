/*
 * L4 authorization server for Medusa DS9
 * Copyright (C) 2002 Milan Pikula <www@terminus.sk>, all rights reserved.
 *
 * This program comes with both BSD and GNU GPL v2 licenses. Check the
 * documentation for more information.
 * 
 *
 * This server communicates with an user-space
 * authorization daemon, using a character device
 *
 *      /dev/medusa c 111 0		on Linux
 *      /dev/medusa c 90 0		on NetBSD
 */

/* define this if you want fatal protocol errors to cause segfault of
 * auth. daemon. Note that issuing strange read(), write(), or trying
 * to access the character device multiple times at once is not considered
 * a protocol error. This triggers only if we REALLY get some junk from the
 * user-space.
 */
#define ERRORS_CAUSE_SEGFAULT

/* define this to support workaround of decisions for named process. This
 * is especially usefull when using GDB on constable.
 */
#define GDB_HACK

/* TODO: Check the calls to l3; they can't be called from a lock. */


#include <linux/medusa/l3/arch.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/poll.h>
//#include <linux/devfs_fs_kernel.h>
#include <linux/semaphore.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#define MEDUSA_MAJOR 111
#define MODULENAME "chardev/linux"
#define wakeup(p) wake_up(p)
#define CURRENTPTR current

#include <linux/medusa/l3/registry.h>
#include <linux/medusa/l3/server.h>
#include <linux/medusa/l4/comm.h>

#include "teleport.h"


static teleport_t teleport = {
cycle: tpc_HALT,
};
static teleport_insn_t tele_mem[6];

/* constable, our brave userspace daemon */
static atomic_t constable_present = ATOMIC_INIT(0);
static struct task_struct * constable = NULL;
static struct task_struct * gdb = NULL;
static MED_LOCK_DATA(constable_openclose);


/* fetch or update answer */
static atomic_t send_fetch_or_update_answer = ATOMIC_INIT(0);
static struct medusa_kclass_s * answ_kclass;
static struct medusa_kobject_s * answ_kobj;
static MCPptr_t answ_kclassid;
static MCPptr_t answ_seq;
static medusa_answer_t answ_result;

/* to-register queue for constable */
static MED_LOCK_DATA(registration_lock);
static struct medusa_kclass_s * kclasses_to_register = NULL;
static struct medusa_evtype_s * evtypes_to_register = NULL;
/* the following two are circular lists */
static struct medusa_kclass_s * kclasses_registered = NULL;
static struct medusa_evtype_s * evtypes_registered = NULL;
static atomic_t announce_ready = ATOMIC_INIT(0);

/* a question from kernel to constable */
static DEFINE_SEMAPHORE(constable_mutex);
static atomic_t question_ready = ATOMIC_INIT(0);
static struct medusa_event_s * decision_event;
static struct medusa_kobject_s * decision_o1, * decision_o2;
/* and the answer */
static medusa_answer_t user_answer;
static DECLARE_WAIT_QUEUE_HEAD(userspace);
static DECLARE_COMPLETION(userspace_answer);

/* is the user-space currently sending us something? */
static atomic_t currently_receiving = ATOMIC_INIT(0);
static char recv_buf[32768]; /* hopefully enough */
static MCPptr_t recv_type;
static int recv_phase;

static DECLARE_WAIT_QUEUE_HEAD(close_wait);

#ifdef GDB_HACK
static pid_t gdb_pid = -1;
//MODULE_PARM(gdb_pid, "i");
//MODULE_PARM_DESC(gdb_pid, "PID to exclude from monitoring");
#endif

/***********************************************************************
 * kernel-space interface
 */

static medusa_answer_t l4_decide( struct medusa_event_s * event,
		struct medusa_kobject_s * o1,
		struct medusa_kobject_s * o2);
static int l4_add_kclass(struct medusa_kclass_s * cl);
static int l4_add_evtype(struct medusa_evtype_s * at);
static void l4_close_wake(void);

static struct medusa_authserver_s chardev_medusa = {
	MODULENAME,
	0,	/* use-count */
	l4_close_wake,		/* close */
	l4_add_kclass,		/* add_kclass */
	NULL,			/* del_kclass */
	l4_add_evtype,		/* add_evtype */
	NULL,			/* del_evtype */
	l4_decide		/* decide */
};

static void l4_close_wake(void)
{
	wakeup(&close_wait);
}

static int l4_add_kclass(struct medusa_kclass_s * cl)
{
	med_get_kclass(cl);
	MED_LOCK_W(registration_lock);
	cl->cinfo = (cinfo_t)kclasses_to_register; 
	kclasses_to_register=cl;
	barrier();
	atomic_set(&announce_ready, 1);
	barrier();
	wakeup(&userspace);
	MED_UNLOCK_W(registration_lock);
	return 0;
}

static int l4_add_evtype(struct medusa_evtype_s * at)
{
	MED_LOCK_W(registration_lock);
	at->cinfo = (cinfo_t)evtypes_to_register; 
	evtypes_to_register=at;
	barrier();
	atomic_set(&announce_ready, 1);
	barrier();
	wakeup(&userspace);
	MED_UNLOCK_W(registration_lock);
	return 0;
}

/* the sad fact about this routine is that it sleeps...
 *
 * guess what? we can FULLY solve that silly problem on SMP,
 * eating one processor by a constable... ;) One can imagine
 * the performance improvement, and buy one more CPU in advance :)
 */
static medusa_answer_t l4_decide(struct medusa_event_s * event,
		struct medusa_kobject_s * o1, struct medusa_kobject_s * o2)
{
	int retval;
	if (!in_task()) {
		/* houston, we have a problem! */
		med_pr_err("decide called from interrupt context :(\n");
		return MED_ERR;
	}
	if (current == constable || current == gdb)
		return MED_OK;

	if (current->pid < 1)
		return MED_ERR;
#ifdef GDB_HACK
	if (gdb_pid == current->pid)
		return MED_OK;
#endif

	if (down_killable(&constable_mutex)) // Don't create kill resistent program if there is an error in constable...
		return MED_NO;

	/* end before sleeping, if possible */
	if (!atomic_read(&constable_present)) {
		/* because of Linux implementation of semaphores,
		 * this path is pretty fast and won't affect SMP
		 * much, when constable is off.
		 */
		up(&constable_mutex);
		return MED_ERR;
	}

	/* place the question and ask. */
	decision_event = event;
	decision_o1 = o1;
	decision_o2 = o2;
	/* wmb() */
	barrier(); /* gcc optimalization causes segfault on multiprocessor machines */
	atomic_set(&question_ready, 1); /* doesn't matter whether this is atomic or not */
	med_pr_debug("medusa: question ready = 1\n");
	barrier();
	wakeup(&userspace);
	barrier();
	med_pr_debug("medusa: goin sleep\n");
	wait_for_completion(&userspace_answer);
	med_pr_debug("medusa: wakeup woe\n");
	barrier();
	if (atomic_read(&question_ready)) {
		atomic_set(&question_ready, 0);
		med_pr_info("medusa: race conditions %d...\n", userspace_answer.done);
	} else {
		med_pr_info("medusa: no race conditions %d...\n", userspace_answer.done);
	}

	if (atomic_read(&constable_present))
		retval = user_answer;
	else
		retval = MED_ERR;
	barrier();
	decision_event = NULL;
	decision_o1 = NULL;
	decision_o2 = NULL;
	up(&constable_mutex);
	return retval;
}

/***********************************************************************
 * user-space interface
 */

static ssize_t user_read(struct file *filp, char *buf, size_t count, loff_t * ppos);
static ssize_t user_write(struct file *filp, const char *buf, size_t count, loff_t * ppos);
static unsigned int user_poll(struct file *filp, poll_table * wait);
static int user_open(struct inode *inode, struct file *file);
static int user_release(struct inode *inode, struct file *file);

static struct file_operations fops = {
read:		user_read,
		write:		user_write,
		llseek:		no_llseek, /* -ESPIPE */
		poll:		user_poll,
		open:		user_open,
		release:	user_release
			/* we don't support async IO. I have no idea, when to call kill_fasync
			 * to be correct. Only on decisions? Or also on answers to user-space
			 * questions? Not a big problem, though... noone seems to be supporting
			 * it anyway :). If you need it, let me know. <www@terminus.sk>
			 */
			/* also, we don't like the ioctl() - we hope the character device can
			 * be used over the network.
			 */
};
static char * userspace_buf;

static ssize_t to_user(void * from, size_t len)
{ /* we verify the access rights elsewhere */
	if (__copy_to_user(userspace_buf, from, len));
	userspace_buf += len;
	return len;
}

/*
 * READ()
 */
#define XFER_COUNT count
static ssize_t user_read(struct file * filp, char * buf,
		size_t count, loff_t * ppos)
{
	ssize_t retval;
	if (constable != current)
		return -EPERM;
	if (*ppos != filp->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_WRITE, buf, count))
		return -EFAULT;

	/* do we have an unfinished write? (e.g. dumb user-space) */
	if (atomic_read(&currently_receiving))
		return -EIO;
	userspace_buf = buf;
feed_lions:
	retval = teleport_cycle(&teleport, XFER_COUNT);
	if (retval < 0) /* unexpected error; data lost */
		return retval;
	if (retval > 0 || teleport.cycle != tpc_HALT)
		return retval;


	// l4->constable: Fetch object - answer
	// locked by the fact we're in the context of Constable
	if (atomic_read(&send_fetch_or_update_answer))
		goto do_fetch_update; /* the common case goes faster */

	retval = wait_event_interruptible(userspace,
			atomic_read(&announce_ready)||atomic_read(&question_ready));
	if (retval != 0) /* -ERESTARTSYS */
		return retval;

	if (atomic_read(&announce_ready))
		goto do_announce; /* the common case goes faster */


	/* question_ready */
#define decision_evtype ((struct medusa_evtype_s*)decision_event->evtype_id.ptr)
	tele_mem[0].opcode = tp_PUTPtr;
	tele_mem[0].args.putPtr.what = decision_event->evtype_id; // possibility to encryption JK march 2015
	tele_mem[1].opcode = tp_PUT32;
	tele_mem[1].args.put32.what = 0;
	tele_mem[2].opcode = tp_CUTNPASTE;
	tele_mem[2].args.cutnpaste.from = (unsigned char *)decision_event;
	tele_mem[2].args.cutnpaste.count = decision_evtype->event_size;
	tele_mem[3].opcode = tp_CUTNPASTE;
	tele_mem[3].args.cutnpaste.from = (unsigned char *)decision_o1;
	tele_mem[3].args.cutnpaste.count =
		((struct medusa_kclass_s*)decision_evtype->arg_kclass[0].ptr)->kobject_size;
	if (decision_o1 == decision_o2) {
		tele_mem[4].opcode = tp_HALT;
	} else {
		tele_mem[4].opcode = tp_CUTNPASTE;
		tele_mem[4].args.cutnpaste.from =
			(unsigned char *)decision_o2;
		tele_mem[4].args.cutnpaste.count =
			((struct medusa_kclass_s*)decision_evtype->arg_kclass[1].ptr)->kobject_size;
		tele_mem[5].opcode = tp_HALT;
	}
#undef decision_evtype
	teleport_reset(&teleport, &(tele_mem[0]), to_user);
	med_pr_debug("medusa: question ready = 0");
	atomic_set(&question_ready, 0);
	goto feed_lions;

do_fetch_update:
	tele_mem[0].opcode = tp_PUTPtr;
	tele_mem[0].args.putPtr.what.data = (uint64_t)0;
	tele_mem[1].opcode = tp_PUT32;
	if (atomic_read(&send_fetch_or_update_answer) == MEDUSA_COMM_FETCH_REQUEST) { /* fetch */
		tele_mem[1].args.put32.what = answ_kobj ?
			MEDUSA_COMM_FETCH_ANSWER : MEDUSA_COMM_FETCH_ERROR;
	} else { /* update */
		tele_mem[1].args.put32.what = MEDUSA_COMM_UPDATE_ANSWER;
	}
	tele_mem[2].opcode = tp_PUTPtr;
	tele_mem[2].args.putPtr.what = answ_kclassid;
	tele_mem[3].opcode = tp_PUTPtr;
	tele_mem[3].args.putPtr.what = answ_seq;
	if (atomic_read(&send_fetch_or_update_answer) == MEDUSA_COMM_UPDATE_REQUEST) {
		tele_mem[4].opcode = tp_PUT32;
		tele_mem[4].args.put32.what = answ_result;
		tele_mem[5].opcode = tp_HALT;
	} else if (answ_kobj) {
		tele_mem[4].opcode = tp_CUTNPASTE;
		tele_mem[4].args.cutnpaste.from = (void *)answ_kobj;
		tele_mem[4].args.cutnpaste.count = answ_kclass->kobject_size;
		tele_mem[5].opcode = tp_HALT;
	} else
		tele_mem[4].opcode = tp_HALT;
	med_put_kclass(answ_kclass); /* slightly too soon */
	teleport_reset(&teleport, &(tele_mem[0]), to_user);
	atomic_set(&send_fetch_or_update_answer, 0);
	goto feed_lions;

do_announce:
	/* announce_ready */
	MED_LOCK_W(registration_lock);
	if (kclasses_to_register) {
		struct medusa_kclass_s * p;

		p = kclasses_to_register;
		kclasses_to_register = (struct medusa_kclass_s *)p->cinfo;

		p->cinfo = (cinfo_t)kclasses_registered;
		kclasses_registered = p;

		tele_mem[0].opcode = tp_PUTPtr;
		tele_mem[0].args.putPtr.what.data = 0;
		tele_mem[1].opcode = tp_PUT32;
		tele_mem[1].args.put32.what =
			MEDUSA_COMM_KCLASSDEF;
		tele_mem[2].opcode = tp_PUTKCLASS;
		tele_mem[2].args.putkclass.kclassdef = p;
		tele_mem[3].opcode = tp_PUTATTRS;
		tele_mem[3].args.putattrs.attrlist = p->attr;
		tele_mem[4].opcode = tp_HALT;
	} else if (evtypes_to_register) {
		struct medusa_evtype_s * p;

		p = evtypes_to_register;
		evtypes_to_register = (struct medusa_evtype_s *)p->cinfo;

		p->cinfo = (cinfo_t)evtypes_registered;
		evtypes_registered = p;

		tele_mem[0].opcode = tp_PUTPtr;
		tele_mem[0].args.putPtr.what.data = 0;
		tele_mem[1].opcode = tp_PUT32;
		tele_mem[1].args.put32.what =
			MEDUSA_COMM_EVTYPEDEF;
		tele_mem[2].opcode = tp_PUTEVTYPE;
		tele_mem[2].args.putevtype.evtypedef = p;
		tele_mem[3].opcode = tp_PUTATTRS;
		tele_mem[3].args.putattrs.attrlist = p->attr;
		tele_mem[4].opcode = tp_HALT;
	}
	teleport_reset(&teleport, &(tele_mem[0]), to_user);
	atomic_set(&announce_ready,  (kclasses_to_register || evtypes_to_register));
	MED_UNLOCK_W(registration_lock);
	goto feed_lions;
}

/*
 * WRITE()
 */
# define GET_UPTO(to_read) do {							\
	if (howmuch > (signed int)count)					\
	howmuch = count;						\
	if (howmuch <= 0)							\
	break;								\
	if (__copy_from_user(recv_buf+recv_phase-sizeof(MCPptr_t), buf, howmuch));	\
	buf += howmuch; count -= howmuch; recv_phase += howmuch;		\
} while (0)

static ssize_t user_write(struct file *filp, const char *buf, size_t count, loff_t * ppos)
{
	size_t orig_count = count;
	struct medusa_kclass_s * cl;

	if (constable != current)
		return -EPERM;
	if (*ppos != filp->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_READ, buf, count))
		return -EFAULT;

	while (XFER_COUNT) {
		if (!atomic_read(&currently_receiving)) {
			recv_phase = 0;
			atomic_set(&currently_receiving, 1);
		}
		if (recv_phase < sizeof(MCPptr_t)) {
			int to_read = sizeof(MCPptr_t) - recv_phase;
			if (to_read > XFER_COUNT)
				to_read = XFER_COUNT;
			if (__copy_from_user(((char *)&recv_type)+recv_phase, buf,
						to_read));
			buf += to_read; count -= to_read;
			recv_phase += to_read;
			if (recv_phase < sizeof(MCPptr_t))
				continue;
		}
		if (recv_type.data == MEDUSA_COMM_AUTHANSWER) {
			int size = (2*sizeof(MCPptr_t)+ sizeof(uint16_t));
			GET_UPTO(size); // TODO change !!!
			if (recv_phase < size)
				continue;
			user_answer = *(int16_t *)(recv_buf+sizeof(MCPptr_t));
			barrier();
			med_pr_debug("medusa: goin complete\n");
			complete(&userspace_answer);
			med_pr_debug("medusa: is complete\n");
			atomic_set(&currently_receiving, 0);
		} else if (recv_type.data == MEDUSA_COMM_FETCH_REQUEST ||
				recv_type.data == MEDUSA_COMM_UPDATE_REQUEST) {
			GET_UPTO(sizeof(MCPptr_t)*3);
			if (recv_phase < sizeof(MCPptr_t)*3)
				continue;

			cl = med_get_kclass_by_pointer(
					*(struct medusa_kclass_s **)(recv_buf) // posibility to decrypt JK march 2015
					);
			if (!cl) {
				med_pr_err("Protocol error at write(): unknown kclass 0x%16llx!\n", (*(MCPptr_t*)(recv_buf)).data);
				atomic_set(&currently_receiving, 0);
#ifdef ERRORS_CAUSE_SEGFAULT
				return -EFAULT;
#else
				break;
#endif
			}
			GET_UPTO(sizeof(MCPptr_t)*3+cl->kobject_size);
			if (recv_phase < sizeof(MCPptr_t)*3+cl->kobject_size) {
				med_put_kclass(cl);
				continue;
			}
			if (atomic_read(&send_fetch_or_update_answer)) {
				/* not so much to do... */
				med_put_kclass(answ_kclass);
			}
			answ_kclass = cl;
			if (recv_type.data == MEDUSA_COMM_FETCH_REQUEST) {
				if (cl->fetch)
					answ_kobj = cl->fetch((struct medusa_kobject_s *)
							(recv_buf+sizeof(MCPptr_t)*2));
				else
					answ_kobj = NULL;
			} else {
				if (cl->update)
					answ_result = cl->update(
							(struct medusa_kobject_s *)(recv_buf+sizeof(MCPptr_t)*2));
				else
					answ_result = MED_ERR;
			}
			answ_kclassid = (*(MCPptr_t*)(recv_buf));
			answ_seq = *(((MCPptr_t*)(recv_buf))+1);
			atomic_set(&send_fetch_or_update_answer, recv_type.data);

			atomic_set(&currently_receiving, 0);
		} else {
			med_pr_err("Protocol error at write(): unknown command %16llx!\n", recv_type.data);
			atomic_set(&currently_receiving, 0);
#ifdef ERRORS_CAUSE_SEGFAULT
			return -EFAULT;
#endif
		}
	}
	return orig_count;
}

/*
 * POLL()
 */
static unsigned int user_poll(struct file *filp, poll_table * wait)
{
	if (constable != current)
		return -EPERM;
	poll_wait(filp, &userspace, wait);
	if (atomic_read(&currently_receiving))
		return POLLOUT | POLLWRNORM;
	if (teleport.cycle != tpc_HALT)
		return POLLIN | POLLRDNORM;
	if (atomic_read(&send_fetch_or_update_answer) || atomic_read(&announce_ready) || atomic_read(&question_ready))
		return POLLIN | POLLRDNORM;
	return POLLOUT | POLLWRNORM;
}

/*
 * OPEN()
 */
static int user_open(struct inode *inode, struct file *file)
{
	int retval = -EPERM;

	//MOD_INC_USE_COUNT; Not needed anymore JK

	MED_LOCK_W(constable_openclose);
	if (atomic_read(&constable_present))
		goto out;

	constable = CURRENTPTR;
	if (strstr(current->parent->comm, "gdb"))
		gdb = current->parent;

	atomic_set(&currently_receiving, 0);
	tele_mem[0].opcode = tp_PUTPtr;
	tele_mem[0].args.putPtr.what.data = MEDUSA_COMM_GREETING;
	tele_mem[1].opcode = tp_HALT;
	teleport_reset(&teleport, &(tele_mem[0]), to_user);

	/* this must be the last thing done */
	atomic_set(&constable_present, 1);
	MED_UNLOCK_W(constable_openclose);

	evtypes_to_register = NULL;
	kclasses_to_register = NULL;

	med_pr_debug("medusa: open: init completiton\n");
	init_completion(&userspace_answer);
	med_pr_debug("medusa: open: init completiton complete\n");

	MED_REGISTER_AUTHSERVER(chardev_medusa);
	return 0; /* success */
out:
	MED_UNLOCK_W(constable_openclose);
	return retval;
}

/*
 * CLOSE()
 */
static int user_release(struct inode *inode, struct file *file)
{
	DECLARE_WAITQUEUE(wait,current);

	if (constable != CURRENTPTR)
		return 0; /* ;) */
	MED_LOCK_W(registration_lock);
	if (evtypes_registered) {
		struct medusa_evtype_s * p1, * p2;
		p1 = evtypes_registered;
		do {
			p2 = p1;
			p1 = (struct medusa_evtype_s *)p1->cinfo;
			// med_put_evtype(p2);
		} while (p1);
	}
	evtypes_registered = NULL;
	if (kclasses_registered) {
		struct medusa_kclass_s * p1, * p2;
		p1 = kclasses_registered;
		do {
			p2 = p1;
			p1 = (struct medusa_kclass_s *)p1->cinfo;
			med_put_kclass(p2);
		} while (p1);
	}
	kclasses_registered = NULL;
	MED_UNLOCK_W(registration_lock);
	if (atomic_read(&send_fetch_or_update_answer)) {
		med_put_kclass(answ_kclass);
		atomic_set(&send_fetch_or_update_answer, 0);
	}

	med_pr_debug("Security daemon unregistered.\n");
#if defined(CONFIG_MEDUSA_HALT)
	med_pr_warn("No security daemon, system halted.\n");
	notifier_call_chain(&reboot_notifier_list, SYS_HALT, NULL);
	machine_halt();
#elif defined(CONFIG_MEDUSA_REBOOT)
	med_pr_warn("No security daemon, rebooting system.\n");
	ctrl_alt_del();
#endif
	add_wait_queue(&close_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);
	MED_UNREGISTER_AUTHSERVER(chardev_medusa);
	MED_LOCK_W(constable_openclose);
	atomic_set(&constable_present, 0);
	constable = NULL;

	user_answer = MED_ERR; // XXXXX change to MED_ERR !!
	med_pr_debug("medusa: close: goin complete \n");
	complete(&userspace_answer);	/* the one which already might be in */
	med_pr_debug("meduse: close: is complete\n");

	atomic_set(&question_ready, 0);
	med_pr_debug("medusa: close question ready = 0");
	atomic_set(&announce_ready, 0);
	MED_UNLOCK_W(constable_openclose);
	schedule();
	remove_wait_queue(&close_wait, &wait);
	//MOD_DEC_USE_COUNT; Not needed anymore? JK
	return 0;
}

static struct class* medusa_class;
static struct device* medusa_device;

static int chardev_constable_init(void)
{
	med_pr_debug("Registering L4 character device with major %d\n", MEDUSA_MAJOR);
	if (register_chrdev(MEDUSA_MAJOR, MODULENAME, &fops)) {
		med_pr_err("Cannot register character device with major %d\n", MEDUSA_MAJOR);
		return -1;
	}
	//devfs_register(NULL, "medusa", DEVFS_FL_DEFAULT, JK???
	//	MEDUSA_MAJOR, 0,
	//	S_IFCHR | S_IRUSR | S_IWUSR ,
	//	&fops, NULL);
	// JK begin create node

	medusa_class = class_create(THIS_MODULE, "medusa");
	if (IS_ERR(medusa_class)) {
		med_pr_err("Failed to register device class '%s'\n", "medusa");
		return -1;
	}

	/* With a class, the easiest way to instantiate a device is to call device_create() */
	medusa_device = device_create(medusa_class, NULL, MKDEV(MEDUSA_MAJOR, 0), NULL, "medusa");
	if (IS_ERR(medusa_device)) {
		med_pr_err("Failed to create device '%s'\n", "medusa");
		return -1;
	}
	return 0;
}

static void chardev_constable_exit(void)
{
	//	devfs_unregister(devfs_find_handle // ??? JK
	//			(NULL, "medusa", MEDUSA_MAJOR, 0,
	//			DEVFS_SPECIAL_CHR, 0));


	device_destroy(medusa_class, MKDEV(MEDUSA_MAJOR, 0));
	class_unregister(medusa_class);
	class_destroy(medusa_class);

	unregister_chrdev(MEDUSA_MAJOR, MODULENAME);
}

module_init(chardev_constable_init);
module_exit(chardev_constable_exit);
MODULE_LICENSE("GPL");

