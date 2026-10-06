/*
#define DEBUG
 */

#include "package.h"

int
main(int argc, char **argv)
{
#ifdef FLATPAK
	/* In flatpak builds, don't pick up VIPSHOME from the environ, we want the
	 * value detected for this install.
	 */
	g_unsetenv("VIPSHOME");
#endif /*FLATPAK*/

	// disable DoS limits on libvips 8.19+
	g_setenv("VIPS_UNLIMITED", "1", TRUE);

#ifdef ENABLE_NLS
    textdomain(GETTEXT_PACKAGE);
#endif /* ENABLE_NLS */
    setlocale(LC_ALL, "");

#ifdef G_OS_WIN32
	/* On Windows, argv is ascii-only ... use this to get a utf-8 version of
	 * the args.
	 */
	argv = g_win32_get_command_line();
	argc = g_strv_length(argv);
#endif /*G_OS_WIN32*/

	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

#ifdef DEBUG
	printf("DEBUG on in main.c\n");
	vips_leak_set(TRUE);
	vips_cache_set_max(0);

	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING);

	g_setenv("G_DEBUG", "fatal-warnings", FALSE);
#endif /*DEBUG*/

	App *app = app_new();

	int status = g_application_run(G_APPLICATION(app), argc, argv);

	vips_shutdown();

	return status;
}
