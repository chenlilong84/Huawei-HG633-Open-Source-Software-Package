/* vi: set sw=4 ts=4: */
/*
 * Mini sulogin implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <syslog.h>

#include "libbb.h"

//static void catchalarm(int ATTRIBUTE_UNUSED junk)
//{
//	exit(EXIT_FAILURE);
//}


int sulogin_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sulogin_main(int argc, char **argv)
{
	char *cp;
	int timeout = 0;
	char *timeout_arg;
	struct passwd *pwd;
	const char *shell;
#if ENABLE_FEATURE_SHADOWPASSWDS
	/* Using _r function to avoid pulling in static buffers */
	char buffer[256];
	struct spwd spw;
#endif

	logmode = LOGMODE_BOTH;
	openlog(applet_name, 0, LOG_AUTH);

	if (getopt32(argv, "t:", &timeout_arg)) {
		timeout = xatoi_u(timeout_arg);
	}

	if (argv[optind]) {
		close(0);
		close(1);
		dup(xopen(argv[optind], O_RDWR));
		close(2);
		dup(0);
	}

	if (!isatty(0) || !isatty(1) || !isatty(2)) {
		logmode = LOGMODE_SYSLOG;
		bb_error_msg_and_die("not a tty");
	}

	/* Clear dangerous stuff, set PATH */
	sanitize_env_for_suid();

// bb_askpass() already handles this
//	signal(SIGALRM, catchalarm);

	pwd = getpwuid(0);
	if (!pwd) {
		goto auth_error;
	}

#if ENABLE_FEATURE_SHADOWPASSWDS
	{
		/* getspnam_r may return 0 yet set result to NULL.
		 * At least glibc 2.4 does this. Be extra paranoid here. */
		struct spwd *result = NULL;
		int r = getspnam_r(pwd->pw_name, &spw, buffer, sizeof(buffer), &result);
		if (r || !result) {
			goto auth_error;
		}
		pwd->pw_passwd = result->sp_pwdp;
	}
#endif

	while (1) {
		/* cp points to a static buffer that is zeroed every time */
		cp = bb_askpass(timeout,
				"Give root password for system maintenance\n"
				"(or type Control-D for normal startup):");

		if (!cp || !*cp) {
			bb_info_msg("Normal startup");
			return 0;
		}
		if (strcmp(pw_encrypt(cp, pwd->pw_passwd), pwd->pw_passwd) == 0) {
			break;
		}
		bb_do_delay(FAIL_DELAY);
		bb_error_msg("login incorrect");
	}
	memset(cp, 0, strlen(cp));
//	signal(SIGALRM, SIG_DFL);

	bb_info_msg("System Maintenance Mode");

	USE_SELINUX(renew_current_security_context());

	shell = getenv("SUSHELL");
	if (!shell)
		shell = getenv("sushell");
	if (!shell) {
		shell = "/bin/sh";
		if (pwd->pw_shell[0])
			shell = pwd->pw_shell;
	}
	/* Exec login shell with no additional parameters. Never returns. */
	run_shell(shell, 1, NULL, NULL);

 auth_error:
	bb_error_msg_and_die("no password entry for root");
}
