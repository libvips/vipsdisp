/*
#define DEBUG
 */

#include "vipsdisp.h"

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

	if (VIPS_INIT(argv[0]))
		vips_error_exit("unable to start libvips");

#ifdef DEBUG
	printf("DEBUG on in main.c\n");
	vips_leak_set(TRUE);

	g_log_set_always_fatal(
		G_LOG_FLAG_RECURSION |
		G_LOG_FLAG_FATAL |
		G_LOG_LEVEL_ERROR |
		G_LOG_LEVEL_CRITICAL |
		G_LOG_LEVEL_WARNING |
		0);

	g_setenv("G_DEBUG", "fatal-warnings", FALSE);
#endif /*DEBUG*/

	VipsdispApp *app = vipsdisp_app_new();

	int status = g_application_run(G_APPLICATION(app), argc, argv);

	vips_shutdown();

	return status;
}
