#
# Makefile for the Linux kernel part of Medusa DS9 Security System
#

# for GCOV coverage profiling
ifdef CONFIG_SECURITY_MEDUSA_GCOV_PROFILE
GCOV_PROFILE := y
endif

O_TARGET := l2.o

export-objs :=

# note that the order is important here!

# kclass definitions, function to manipulate kobjects
obj-y += kobject_process.o
obj-y += kobject_file.o
obj-y += kobject_printk.o
obj-y += kobject_fuck.o
obj-y += kobject_ipc.o
obj-y += kobject_socket.o
obj-y += kobject_mount.o
#obj-$(CONFIG_MEDUSA_FORCE) += kobject_force.o
#obj-$(CONFIG_MEDUSA_MEMKOBJECT) += kobject_memory.o kobject_cstrmem.o
#obj-y += kobject_force.o
obj-y += kobject_memory.o kobject_cstrmem.o
# access types
obj-y += acctype_afterexec.o acctype_capable.o acctype_create.o
obj-y += acctype_exec.o acctype_fork.o
obj-y += acctype_init_process.o acctype_link.o
obj-y += acctype_lookup.o acctype_mkdir.o acctype_mknod.o
obj-y += acctype_notify_change.o
obj-y += acctype_permission.o acctype_readwrite.o
obj-y += acctype_rename.o acctype_rmdir.o
obj-$(CONFIG_SECURITY_MEDUSA_HOOKS_TASK_KILL) += acctype_sendsig.o
obj-y += acctype_setresuid.o acctype_sexec.o
obj-y += acctype_symlink.o acctype_readlink.o
obj-y += acctype_truncate.o acctype_unlink.o
obj-y += acctype_chmod.o acctype_chown.o acctype_chroot.o
obj-y += acctype_fcntl.o acctype_open.o
obj-y += acctype_ipc_permission.o acctype_ipc_ctl.o acctype_ipc_associate.o
obj-y += acctype_ipc_semop.o acctype_ipc_shmat.o acctype_ipc_msgsnd.o acctype_ipc_msgrcv.o
obj-y += acctype_socket_create.o acctype_socket_bind.o acctype_socket_connect.o acctype_socket_listen.o acctype_socket_accept.o
obj-y += acctype_socket_sendmsg.o acctype_socket_recvmsg.o
# arch-dependent files
obj-y += acctype_ptrace.o
obj-y += acctype_mount.o
obj-y += acctype_umount.o
#obj-$(CONFIG_MEDUSA_SYSCALL) += acctype_syscall.o
#obj-y += acctype_syscall.o
# event types
obj-y += evtype_getprocess.o evtype_getfile.o evtype_fuck.o evtype_getipc.o evtype_getsocket.o evtype_getmount.o
# audit
obj-y += audit_medusa.o
