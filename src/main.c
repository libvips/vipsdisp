// common C startup shared by gui and batch executables

/*

    Copyright (C) 1991-2003 The National Gallery
    Copyright (C) 2004-2023 libvips.org

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*
#define DEBUG
#define DEBUG_FATAL
 */

#include "nip4.h"

/* General stuff.
 */
Workspaceroot *main_workspaceroot = NULL;   /* All the workspaces */
Toolkitgroup *main_toolkitgroup = NULL;     /* All the toolkits */
Symbol *main_symbol_root = NULL;            /* Root of symtable */
Watchgroup *main_watchgroup = NULL;         /* All of the watches */
Imageinfogroup *main_imageinfogroup = NULL; /* All of the images */

/* Other parts of nip4 need these to link, but we don't use them here ... see
 * main-batch.c
 */
gboolean main_option_i18n = FALSE;
gboolean main_option_batch = FALSE;
gboolean main_option_time_save = FALSE;
gboolean main_option_profile = FALSE;
gboolean main_option_no_load_menus = FALSE;

const char *main_argv0 = NULL; /* argv[0] */

static char prefix_buffer[VIPS_PATH_MAX];
static gboolean prefix_valid = FALSE;
static void *main_c_stack_base = NULL;      /* Base of C stack */

/* Override the install guess from vips. Handy for testing.
 */
void
set_prefix(const char *prefix)
{
    g_strlcpy(prefix_buffer, prefix, VIPS_PATH_MAX);
    nativeize_path(prefix_buffer);
    absoluteize_path(prefix_buffer);
    setenvf("VIPSHOME", "%s", prefix_buffer);
    prefix_valid = TRUE;
}

/* Guess VIPSHOME, if we can.
 */
const char *
get_prefix(void)
{
    if (!prefix_valid) {
        g_autofree char *basename = g_path_get_basename(main_argv0);
        g_autofree char *argv0_path = NULL;

        const char *prefix;

        prefix = vips_guess_prefix(main_argv0, "VIPSHOME");

        if (!prefix ||
            !existsf("%s/share/nip4", prefix)) {
            /* The libvips guesser failed to find our install area ... try
             * searching the path for our exe name.
             *
             * This can happen with homebrew, for example, where the
             * compile-time libvips prefix will not match the nip4 prefix.
             */
            GSList *path = path_parse(g_getenv("PATH"));
            argv0_path =
                (char *) path_map(path, basename, (path_map_fn) g_strdup, NULL);
            g_slist_free_full(g_steal_pointer(&path), g_free);

            g_autofree char *trailing = g_strjoin("/", "bin", basename, NULL);
            if (argv0_path &&
                is_postfix(trailing, argv0_path))
                argv0_path[strlen(argv0_path) - strlen(trailing)] = '\0';

            prefix = argv0_path;
        }

        if (!prefix ||
            !existsf("%s/share/nip4", prefix)) {
            error_top(_("Unable to find install area"));
            error_vips();

            return NULL;
        }

        set_prefix(prefix);
    }

    return prefix_buffer;
}

/* Make sure a savedir exists. Used to build the "~/.nip2-xx/tmp" etc.
 * directory tree.
 */
static void
main_mkdir(const char *dir)
{
    if (!existsf("%s/%s", get_savedir(), dir))
        if (!mkdirf("%s/%s", get_savedir(), dir))
            vips_error_exit(_("unable to make %s %s: %s"),
                get_savedir(), dir, g_strerror(errno));
}

static void *
main_load_def(const char *filename)
{
    g_autofree char *basename = g_path_get_basename(filename);

    if (!main_option_no_load_menus ||
        basename[0] == '_') {
        Toolkit *kit;

        if (!(kit = toolkit_new_from_file(main_toolkitgroup, filename)))
            error_alert(NULL);
        else
            filemodel_set_auto_load(FILEMODEL(kit));
    }

    return NULL;
}

static void *
main_load_wsg(const char *filename)
{
    Workspacegroup *wsg;

    if (!(wsg = workspacegroup_new_from_file(main_workspaceroot,
              filename, filename)))
        error_alert(NULL);
    else
        filemodel_set_auto_load(FILEMODEL(wsg));

    return NULL;
}

/* NULL log handler. Used to suppress output on win32 without DEBUG_FATAL.
 */
#ifndef DEBUG_FATAL
#ifdef G_OS_WIN32
static void
main_log_null(const char *log_domain, GLogLevelFlags log_level,
    const char *message, void *user_data)
{
}
#endif /*G_OS_WIN32*/
#endif /*!DEBUG_FATAL*/

/* We set the C stack to 2mb during startup (if we can).
 */
gboolean
main_is_stack_full(void)
{
    int x = 0;

    if ((char *) main_c_stack_base - (char *) &x > 2000000) {
        printf("HELP!! stack overflow\n");

        error_top(_("Overflow error"));
        error_sub(_("C stack overflow, expression too complex"));

        return TRUE;
    }

    return FALSE;
}

/* Common startup, shared between gui and batch modes.
 */
void
main_startup(int argc, char **argv)
{
    main_argv0 = argv[0];

    // try to spot stack overflow with this
    main_c_stack_base = &argc;

#ifdef DEBUG
    vips_leak_set(TRUE);
#endif /*DEBUG*/

#ifdef DEBUG_FATAL
    /* Set masks for debugging ... stop on any problem.
     */
    g_log_set_always_fatal(
        G_LOG_FLAG_RECURSION |
        G_LOG_FLAG_FATAL |
        G_LOG_LEVEL_ERROR |
        G_LOG_LEVEL_CRITICAL |
        G_LOG_LEVEL_WARNING);

    g_setenv("G_DEBUG", "fatal-warnings", TRUE);
    vips_leak_set(TRUE);
    printf("DEBUG on in main.c\n");
#else /*!DEBUG_FATAL*/
#ifdef G_OS_WIN32
    /* No logging output ... on win32, log output pops up a very annoying
     * console text box.
     */
    g_log_set_handler("GLib",
        G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        main_log_null, NULL);
    g_log_set_handler("Gtk",
        G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        main_log_null, NULL);
    g_log_set_handler(NULL,
        G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        main_log_null, NULL);
#endif /*G_OS_WIN32*/
#endif /*DEBUG_FATAL*/

#ifdef DEBUG
    printf("main: sizeof(HeapNode) == %zd\n", sizeof(HeapNode));

    /* Should be 3 pointers, hopefully.
     */
    if (sizeof(HeapNode) != 3 * sizeof(void *))
        printf("*** struct packing problem!\n");
#endif /*DEBUG*/

    /* Want numeric locale to be "C", so we have C rules for doing
     * double <-> string (ie. no "," for decimal point).
     */
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");

    /* Make sure our LC_NUMERIC setting is not trashed.
     */
    gtk_disable_setlocale();

    /* Pass config.h stuff down to .ws files.
     */
    setenvf("PACKAGE", "%s", PACKAGE);
    setenvf("VERSION", "%s", VERSION);

    /* Name of the dir we store our config stuff in. This can get used by
     * Preferences.ws.
     */
    setenvf("SAVEDIR", "%s", get_savedir());

    /* Path separator on this platform.
     */
    setenvf("SEP", "%s", G_DIR_SEPARATOR_S);

    /* Executable file extension (eg. ".exe" on Windows). Used by some defs.
     */
    setenvf("EXEEXT", "%s", EXEEXT);

#ifdef G_OS_WIN32
    {
        /* No HOME on windows ... make one from HOMEDRIVE and HOMEDIR (via
         * glib).
         */
        const char *home;
        char buf[VIPS_PATH_MAX];

        if (!(home = g_getenv("HOME")))
            home = g_get_home_dir();

        /* We need native paths.
         */
        g_strlcpy(buf, home, VIPS_PATH_MAX);
        nativeize_path(buf);
        setenvf("HOME", "%s", buf);
    }
#endif /*G_OS_WIN32*/

#ifdef HAVE_GETRLIMIT
    /* Make sure we have lots of file descriptors. Some platforms have cur
     * as 256 and max at 1024 to keep stdio happy.
     */
    struct rlimit rlp;
    if (getrlimit(RLIMIT_NOFILE, &rlp) == 0) {
        rlim_t old_limit = rlp.rlim_cur;

        rlp.rlim_cur = rlp.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &rlp) == 0) {
#ifdef DEBUG
            printf("set max file descriptors to %d\n",
                (int) rlp.rlim_max);
#endif /*DEBUG*/
        }
        else if ((int) rlp.rlim_max != -1)
            /* -1 means can't-be-set, at least on os x, so don't
             * warn.
             */
            g_warning(_("unable to change max file descriptors\n"
                        "max file descriptors still set to %d"),
                (int) old_limit);
    }
    else
        g_warning(_("unable to read max file descriptors"));
#endif /*HAVE_GETRLIMIT*/

    error_init();
    path_init();

    /* Always make these in case some get deleted.
     */
    main_mkdir("");
    main_mkdir("tmp");
    main_mkdir("start");
    main_mkdir("data");

    reduce_context = reduce_new();

    main_symbol_root = symbol_root_init();
    iobject_ref_sink(IOBJECT(main_symbol_root));

    main_workspaceroot = workspaceroot_new("Workspaces");
    iobject_ref_sink(IOBJECT(main_workspaceroot));

    main_toolkitgroup = toolkitgroup_new(symbol_root);
    iobject_ref_sink(IOBJECT(main_toolkitgroup));

    main_imageinfogroup = imageinfogroup_new();
    iobject_ref_sink(IOBJECT(main_imageinfogroup));

    /* Add builtin toolkit.
     */
    builtin_init();

    /* We have to init some of our other classes to get them registered
     * with the XML loader.
     */
    /*
    (void) g_type_class_ref(TYPE_CLOCK);
    (void) g_type_class_ref(TYPE_GROUP);
    (void) g_type_class_ref(TYPE_REAL);
    (void) g_type_class_ref(TYPE_VECTOR);
     */

    (void) g_type_class_ref(COLOUR_TYPE);
    (void) g_type_class_ref(COLUMN_TYPE);
    (void) g_type_class_ref(EXPRESSION_TYPE);
    (void) g_type_class_ref(FONTNAME_TYPE);
    (void) g_type_class_ref(IARROW_TYPE);
    (void) g_type_class_ref(IENTRY_TYPE);
    (void) g_type_class_ref(IIMAGE_TYPE);
    (void) g_type_class_ref(IMAGEDISPLAY_TYPE);
    (void) g_type_class_ref(IREGION_TYPE);
    (void) g_type_class_ref(ITEXT_TYPE);
    (void) g_type_class_ref(MATRIX_TYPE);
    (void) g_type_class_ref(NUMBER_TYPE);
    (void) g_type_class_ref(OPTION_TYPE);
    (void) g_type_class_ref(PATHNAME_TYPE);
    (void) g_type_class_ref(PLOT_TYPE);
    (void) g_type_class_ref(PLOTDISPLAY_TYPE);
    (void) g_type_class_ref(RHS_TYPE);
    (void) g_type_class_ref(ROW_TYPE);
    (void) g_type_class_ref(SLIDER_TYPE);
    (void) g_type_class_ref(STRING_TYPE);
    (void) g_type_class_ref(SUBCOLUMN_TYPE);
    (void) g_type_class_ref(TOGGLE_TYPE);
    (void) g_type_class_ref(WORKSPACE_TYPE);

    /* Load up all start defs and wses.
     */
#ifdef DEBUG
    printf("definitions init\n");
#endif /*DEBUG*/
    (void) path_map(PATH_START, "*.def", (path_map_fn) main_load_def, NULL);

    /* Build all the base defs before we load the prefs workspace.
     */
    symbol_recalculate_all_force(TRUE);

#ifdef DEBUG
    printf("ws init\n");
#endif /*DEBUG*/
    (void) path_map(PATH_START, "*.ws", (path_map_fn) main_load_wsg, NULL);

    /* Double-check: we often forget to move the prefs ws to the latest
     * version.
     */
    Symbol *wsr_sym = main_workspaceroot->sym;
    Compile *compile = wsr_sym->expr->compile;
    iContainer *container =
        icontainer_child_lookup(ICONTAINER(compile), "Preferences");

    if (container) {
        Workspace *ws = SYMBOL(container)->ws;

        if (ws->kitg->compat_major ||
            ws->kitg->compat_minor)
            printf("Preferences loaded in compat mode!\n");
    }
    else
        printf("No prefs workspace!\n");

    /* Recalc to build all classes and gets prefs working.
     *
     * We have to do this in batch mode since we can find dirties through
     * dynamic lookups. Even though you might think we could just follow
     * recomps.
     */
    symbol_recalculate_all_force(TRUE);

    /* Expand vips operation cache.
     *
     * FIXME .. link this to prefs?
     */
    vips_cache_set_max(10000);
    vips_cache_set_max_mem(1000 * 1024 * 1024);
    vips_cache_set_max_files(1000);
}

void
main_shutdown(void)
{
    /* Junk all symbols. This may remove a bunch of intermediate images
     * too.
     */
    // UNREF(main_watchgroup);
    VIPS_UNREF(main_symbol_root);
    VIPS_UNREF(main_toolkitgroup);
    VIPS_UNREF(main_workspaceroot);

    /* Junk reduction machine ... this should remove all image temps.
     */
    reduce_destroy(reduce_context);

    /* Trust, but verify.
     */
#ifndef RELEASE
    path_rewrite_free_all();
    VIPS_UNREF(main_imageinfogroup);
    heap_check_all_destroyed();
    managed_check_all_destroyed();
    util_check_all_destroyed();
    view_dump();
    iobject_leak();
#endif /*!RELEASE*/

    /* Can we leave files in tmp after ^C/^V?
     */

    vips_shutdown();
}
