#ifndef PLATINUM_APPLICATION_INTERNAL_H
#define PLATINUM_APPLICATION_INTERNAL_H

/*
 * What the application's files share and nothing else may use. The public
 * interface is application.h.
 *
 * application.c is the event loop: init, run, dispose, the event dispatch and
 * drawing. application_menus.c builds the menu bar and acts on a menu choice;
 * application_views.c opens and refreshes the windows (timeline, thread,
 * profile, people, search, notifications); application_post.c is composing,
 * replying, quoting and engaging; application_auth.c is signing in and out.
 */

#include "application.h"

#include <Events.h>
#include <Fonts.h>
#include <Memory.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "timeline.h"
#include "profile.h"
#include "notifications.h"
#include "preferences.h"
#include "pairing.h"
#include "text_codec.h"
#include "draft.h"
#include "scrollbar.h"
#include "json_min.h"
#include "wolfram/macos9_tls.h"

#define kFileMenuID 128
#define kEditMenuID 129
#define kViewMenuID 130
#define kWindowMenuID 131
#define kHelpMenuID 132
#define kPostMenuID 133

/* application.c */
void platinum_application_yield(void *userdata);
void platinum_application_handle_event(platinum_application *app, EventRecord *event);
void platinum_application_draw(platinum_application *app);
short platinum_application_timeline_visible_rows( const platinum_application *app);
void platinum_application_relayout(platinum_application *app);
void platinum_application_invalidate(platinum_application *app);

/* application_menus.c */
OSErr platinum_application_create_menus(platinum_application *app);
void platinum_application_dispose_menus(platinum_application *app);
void platinum_application_handle_menu(platinum_application *app, long choice);

/* application_views.c */
void platinum_application_refresh_timeline(platinum_application *app);
void platinum_application_show_author(platinum_application *app);
void platinum_application_show_people(platinum_application *app, int menu_item);
void platinum_application_more_people(platinum_application *app);
void platinum_application_run_search(platinum_application *app);
void platinum_application_show_words(platinum_application *app);
void platinum_application_remove_word(platinum_application *app);
void platinum_application_show_posts(platinum_application *app);
void platinum_application_more_posts(platinum_application *app);
void platinum_application_show_named(platinum_application *app, int kind);
void platinum_application_open_person(platinum_application *app);
void platinum_application_follow(platinum_application *app);
void platinum_application_relate(platinum_application *app, int kind);
void platinum_application_show_thread(platinum_application *app);
void platinum_application_open_diag(platinum_application *app);
void platinum_application_check_diag(platinum_application *app);
void platinum_application_load_older(platinum_application *app);
void platinum_application_open_profile(platinum_application *app);
void platinum_application_open_notifications(platinum_application *app);
void platinum_application_older_notifications( platinum_application *app);
void platinum_application_refresh_notifications( platinum_application *app);

/* application_post.c */
void platinum_application_reply(platinum_application *app);
void platinum_application_delete_post(platinum_application *app);
void platinum_application_new_post(platinum_application *app);
void platinum_application_close_compose(platinum_application *app, int sent);
void platinum_application_quote(platinum_application *app);
void platinum_application_engage(platinum_application *app, int repost);
void platinum_application_post_status(platinum_application *app, wf_status status);
void platinum_application_submit_post(platinum_application *app);

/* application_auth.c */
void platinum_application_open_preferences( platinum_application *app);
void platinum_application_sign_out( platinum_application *app);
void platinum_application_recover_auth(platinum_application *app, wf_status status);
OSErr platinum_application_open_pairing( platinum_application *app);
void platinum_application_show_pairing_error( platinum_application *app, const char *message);
void platinum_application_open_apppw(platinum_application *app);
void platinum_application_attempt_apppw(platinum_application *app);
void platinum_application_attempt_pair( platinum_application *app);

#endif
