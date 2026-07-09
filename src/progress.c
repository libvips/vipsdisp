/* Handle feedback about eval progress.
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your watch) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

 */

/*
#define DEBUG
 */

#include "package.h"

#ifdef NIP4
G_DEFINE_TYPE(Progress, progress, IOBJECT_TYPE)
#else /*!NIP4*/
G_DEFINE_TYPE(Progress, progress, G_TYPE_OBJECT)
#endif /*NIP4*/

/* Our signals.
 */
typedef enum _ProgressSignals {
	SIG_BEGIN,	/* Switch to busy state */
	SIG_UPDATE, /* Busy progress update */
	SIG_END,	/* End busy state */
	SIG_LAST
} ProgressSignal;

static guint progress_signals[SIG_LAST] = { 0 };

static void
progress_class_init(ProgressClass *class)
{
	progress_signals[SIG_BEGIN] = g_signal_new("begin",
		G_OBJECT_CLASS_TYPE(class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ProgressClass, begin),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	progress_signals[SIG_UPDATE] = g_signal_new("update",
		G_OBJECT_CLASS_TYPE(class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ProgressClass, update),
		NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	progress_signals[SIG_END] = g_signal_new("end",
		G_OBJECT_CLASS_TYPE(class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(ProgressClass, end),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
progress_init(Progress *progress)
{
#ifdef DEBUG
	printf("progress_init\n");
#endif /*DEBUG*/

	progress->busy_timer = g_timer_new();
	progress->update_timer = g_timer_new();
	progress->cancel = FALSE;
	progress->busy = FALSE;
	vips_buf_init_static(&progress->feedback,
		progress->buf, PROGRESS_FEEDBACK_SIZE);
}

static Progress *
progress_new(void)
{
	Progress *progress = PROGRESS(g_object_new(PROGRESS_TYPE, NULL));

	return progress;
}

Progress *
progress_get(void)
{
	static Progress *progress = NULL;

	if (!progress)
		progress = progress_new();

	return progress;
}

typedef struct _ProgressEvent {
	ProgressSignal signal;
	int percent;
	int eta;
	char *message;
} ProgressEvent;

static ProgressEvent *
progress_event_new(ProgressSignal signal,
	int percent, int eta, const char *message)
{
	ProgressEvent *event = g_new0(ProgressEvent, 1);

	event->signal = signal;
	event->percent = percent;
	event->eta = eta;
	event->message = g_strdup(message);

	return event;
}

static void
progress_event_free(ProgressEvent *event)
{
	VIPS_FREE(event->message);
	VIPS_FREE(event);
}

static void
progress_emit_begin(Progress *progress)
{
	g_signal_emit(G_OBJECT(progress), progress_signals[SIG_BEGIN], 0);
}

static gboolean
progress_emit_update(Progress *progress)
{
	gboolean cancel = FALSE;
	g_signal_emit(progress, progress_signals[SIG_UPDATE], 0, &cancel);

	return cancel;
}

static void
progress_emit_end(Progress *progress)
{
	g_signal_emit(G_OBJECT(progress), progress_signals[SIG_END], 0);
}

static gboolean
progress_event_idle(void *user_data)
{
	ProgressEvent *event = (ProgressEvent *) user_data;
	Progress *progress = progress_get();

	switch (event->signal) {
	case SIG_BEGIN:
		progress->count += 1;

		if (progress->count == 1) {
			g_timer_start(progress->busy_timer);
			g_timer_start(progress->update_timer);
			progress->last_update_time = -1;
		}

		// don't emit BEGIN right away, we want to wait a moment
		break;

	case SIG_UPDATE:
		if (progress->count) {
			double elapsed = g_timer_elapsed(progress->busy_timer, NULL);

			if (!progress->busy &&
				elapsed > 0.5) {
				progress_emit_begin(progress);
				progress->busy = TRUE;
			}
		}

		/* Overwrite the message if we're cancelling.
		 */
		vips_buf_rewind(&progress->feedback);
		if (progress->cancel) {
			vips_buf_appends(&progress->feedback, _("Cancelling"));
			vips_buf_appends(&progress->feedback, " ...");
		}
		else
			vips_buf_appends(&progress->feedback, event->message);

		progress->percent = event->percent;
		progress->eta = event->eta;

		if (progress->busy &&
			progress_emit_update(progress))
			progress->cancel = TRUE;

		break;

	case SIG_END:
		if (progress->count == 1) {
			if (progress->busy)
				progress_emit_end(progress);

			progress->count = 0;
			progress->cancel = FALSE;
			progress->busy = FALSE;
		}
		else
			progress->count = VIPS_MAX(0, progress->count - 1);;
		break;

	default:
		break;
	}

	progress_event_free(event);

	return FALSE;
}

static void
progress_event_signal(ProgressEvent *event)
{
	Progress *progress = progress_get();

	/* Throttle update events to 10Hz.
	 */
	if (event->signal == SIG_UPDATE) {
		double time_now = g_timer_elapsed(progress->update_timer, NULL);

		if (time_now - progress->last_update_time < 0.1) {
			progress_event_free(event);
			return;
		}

		progress->last_update_time = time_now;
	}

	g_idle_add(progress_event_idle, event);
}

void
progress_begin(void)
{
	ProgressEvent *event = progress_event_new(SIG_BEGIN, 0, 0, "");
	progress_event_signal(event);
}

gboolean
progress_update_percent(int percent, int eta)
{
	Progress *progress = progress_get();

	char text[256];
	VipsBuf message = VIPS_BUF_STATIC(text);

	// values of zero make no sense
	eta += 1;

	if (eta > 30) {
		int minutes = (eta + 30) / 60;

		vips_buf_appendf(&message,
			ngettext("%d minute left", "%d minutes left", minutes), minutes);
	}
	else
		vips_buf_appendf(&message,
			ngettext("%d second left", "%d seconds left", eta), eta);

	ProgressEvent *event =
		progress_event_new(SIG_UPDATE, percent, eta, vips_buf_all(&message));
	progress_event_signal(event);

	return progress->cancel;
}

#ifdef NIP4
gboolean
progress_update_expr(Expr *expr)
{
	Progress *progress = progress_get();

	char text[256];
	VipsBuf message = VIPS_BUF_STATIC(text);

	vips_buf_appends(&message, _("Calculating"));
	vips_buf_appends(&message, " ");
	if (expr)
		expr_name(expr, &message);
	else
		vips_buf_appends(&message, symbol_get_last_calc());
	vips_buf_appends(&message, " ...");

	ProgressEvent *event =
		progress_event_new(SIG_UPDATE, 0, 0, vips_buf_all(&message));
	progress_event_signal(event);

	return progress->cancel;
}
#endif /*NIP4*/

gboolean
progress_update_loading(int percent, const char *filename)
{
	Progress *progress = progress_get();

	char text[256];
	VipsBuf message = VIPS_BUF_STATIC(text);

	vips_buf_appends(&message, _("Loading"));
	vips_buf_appendf(&message, " \"%s\"", filename);

	ProgressEvent *event =
		progress_event_new(SIG_UPDATE, percent, 0, vips_buf_all(&message));
	progress_event_signal(event);

	return progress->cancel;
}

gboolean
progress_update_tick(void)
{
	Progress *progress = progress_get();

	ProgressEvent *event = progress_event_new(SIG_UPDATE, 0, 0, "");
	progress_event_signal(event);

	return progress->cancel;
}

void
progress_end(void)
{
	ProgressEvent *event = progress_event_new(SIG_END, 0, 0, "");
	progress_event_signal(event);
}

