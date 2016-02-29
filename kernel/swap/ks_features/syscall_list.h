/**
 * @file ks_features/syscall_list.h
 * @author Vyacheslav Cherkashin: SWAP ks_features implement
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Syscalls list.
 */


#ifndef _SYSCALL_LIST_H
#define _SYSCALL_LIST_H

#define SYSCALL_LIST \
	X(sys_accept4, dpdd), \
	X(sys_accept, dpd), \
	X(sys_access, sd), \
	X(sys_acct, s), \
	X(sys_bind, dpd), \
	X(sys_chdir, s), \
	X(sys_chmod, sd), \
	X(sys_chown16, sdd), \
	X(sys_chown, sdd), \
	X(sys_chroot, s), \
	X(sys_clone, ddddd), \
	X(sys_connect, dpd), \
	X(sys_creat, sd), \
	X(sys_dup3, ddd), \
	X(sys_epoll_create1, d), \
	X(sys_epoll_ctl, dddp), \
	X(sys_epoll_pwait, dpddpx), \
	X(sys_epoll_wait, dpdd), \
	X(sys_eventfd2, dd), \
	X(sys_eventfd, d), \
	X(sys_execve, spp), \
	X(sys_exit, d), \
	X(sys_exit_group, d), \
	X(sys_faccessat, dsd), \
	X(sys_fadvise64_64, dxxd), \
	X(sys_fallocate, ddxx), \
	X(sys_fanotify_init, dd), \
	X(sys_fanotify_mark, ddxds), \
	X(sys_fchmodat, dsd), \
	X(sys_fchownat, dsddd), \
	X(sys_fgetxattr, dspx), \
	X(sys_flistxattr, dpx), \
	X(sys_fork, ), \
	X(sys_fremovexattr, ds), \
	X(sys_fstat64, xp), \
	X(sys_ftruncate64, dx), \
	X(sys_futimesat, dsp), \
	X(sys_getcwd, px), \
	X(sys_getpeername, dpd), \
	X(sys_getsockname, dpd), \
	X(sys_getsockopt, dddpd), \
	X(sys_getxattr, sspx), \
	X(sys_inotify_add_watch, dsd), \
	X(sys_inotify_init, ), \
	X(sys_inotify_init1, d), \
	X(sys_inotify_rm_watch, dd), \
	X(sys_ipc, ddxxpx), \
	X(sys_kill, dd), \
	X(sys_linkat, dsdsd), \
	X(sys_link, ss), \
	X(sys_listen, dd), \
	X(sys_listxattr, spx), \
	X(sys_lstat64, sp), \
/* TODO: X(sys_lstat, sp), */ \
	X(sys_mkdirat, dsd), \
	X(sys_mkdir, sd), \
	X(sys_mknodat, dsdd), \
	X(sys_mknod, sdd), \
	X(sys_mmap_pgoff, xxxxxx), \
	X(sys_mount, pppxp), \
	X(sys_msgctl, ddp), \
	X(sys_msgget, dd), \
	X(sys_msgrcv, dpxxd), \
	X(sys_msgsnd, dpxd), \
	X(sys_name_to_handle_at, dspdd), \
/* TODO: X(sys_newfstatat, dspd), */ \
	X(sys_old_mmap, p), \
	X(sys_openat, dsdd), \
	X(sys_open_by_handle_at, dpd), \
	X(sys_open, sdd), \
	X(sys_pause, ), \
	X(sys_pipe2, dd), \
	X(sys_ppoll, pdpp), \
	X(sys_pread64, dpxx), \
	X(sys_preadv, xpxxx), \
	X(sys_pselect6, dxxxpp), \
	X(sys_pwrite64, dsxx), \
	X(sys_pwritev, xpxxx), \
	X(sys_readlinkat, dspd), \
	X(sys_readlink, spd), \
	X(sys_recv, dpxd), \
	X(sys_recvfrom, dpxdpd), \
	X(sys_recvmmsg, dpddp), \
	X(sys_recvmsg, dpd), \
	X(sys_removexattr, ss), \
	X(sys_renameat, dsds), \
	X(sys_rename, ss), \
	X(sys_rmdir, s), \
	X(sys_rt_sigaction, dpp), \
	X(sys_rt_sigprocmask, dppx), \
	X(sys_rt_sigsuspend, px), \
	X(sys_rt_sigtimedwait, pppx), \
	X(sys_rt_tgsigqueueinfo, dddp), \
	X(sys_semctl, dddx), \
	X(sys_semget, ddd), \
	X(sys_semop, dpd), \
	X(sys_semtimedop, dpdp), \
	X(sys_send, dpxd), \
	X(sys_sendfile64, ddlxx), \
	X(sys_sendfile, ddxx), \
	X(sys_sendmmsg, dpdd), \
	X(sys_sendmsg, dpd), \
	X(sys_sendto, dpxdpd), \
	X(sys_setns, dd), \
	X(sys_setsockopt, dddpd), \
	X(sys_setxattr, sspxd), \
	X(sys_shmat, dpd), \
	X(sys_shmctl, ddp), \
	X(sys_shmdt, p), \
	X(sys_shmget, dxd), \
	X(sys_shutdown, dd), \
	X(sys_sigaction, dpp), \
/* TODO: X(sys_sigaltstack, pp), */ \
/* TODO: X(sys_signal, dp), */ \
	X(sys_signalfd4, dpxd), \
	X(sys_signalfd, dpx), \
	X(sys_sigpending, p), \
	X(sys_sigprocmask, dpp), \
/* TODO: X(sys_sigsuspend, ddp), */ \
/* TODO: X(sys_sigsuspend, p), */ \
	X(sys_socketcall, dx), \
	X(sys_socket, ddd), \
	X(sys_socketpair, dddd), \
	X(sys_splice, dxdxxd), \
	X(sys_stat64, sp), \
	X(sys_statfs64, sxp), \
	X(sys_statfs, sp), \
/* TODO: X(sys_stat, sp), */ \
	X(sys_swapoff, s), \
	X(sys_swapon, sd), \
	X(sys_symlinkat, sds), \
	X(sys_symlink, ss), \
	X(sys_syncfs, d), \
	X(sys_tee, ddxd), \
	X(sys_tgkill, ddd), \
	X(sys_timerfd_create, dd), \
	X(sys_timerfd_gettime, dp), \
	X(sys_timerfd_settime, ddpp), \
	X(sys_truncate64, sx), \
	X(sys_truncate, sx), \
	X(sys_umount, pd), \
	X(sys_unlinkat, dsd), \
	X(sys_unlink, s), \
	X(sys_unshare, x), \
	X(sys_uselib, s), \
	X(sys_utimensat, dspd), \
	X(sys_utime, pp), \
	X(sys_utimes, pp), \
	X(sys_vfork, ), \
	X(sys_vmsplice, dpxd), \
	X(sys_wait4, dddp), \
	X(sys_waitid, ddpdp)
/* TODO: X(sys_waitpid, ddd) */

#endif /* _SYSCALL_LIST_H */
