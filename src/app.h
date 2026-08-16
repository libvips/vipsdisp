#ifndef __APP_H
#define __APP_H

#define APP_TYPE (app_get_type())
#define APP VIPSDISP_APP

G_DECLARE_FINAL_TYPE(App, app, VIPSDISP, APP, GtkApplication)

App *app_new(void);

#endif /*__APP_H*/
