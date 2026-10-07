/* Signing in with a pairing code or an app password, signing out, and recovering from a lost session. */

#include "application_internal.h"

void platinum_application_open_preferences(
    platinum_application *app)
{
    if (app == NULL)
        return;

    platinum_preferences_open(&app->preferences, &app->session);
}

void platinum_application_sign_out(
    platinum_application *app)
{
    wf_status status;

    if (app == NULL)
        return;

    status = platinum_session_sign_out(&app->session);

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_apppw_close(&app->apppw);
    platinum_timeline_init(&app->timeline);
    app->ui.selected_post = 0;
    app->ui.scroll_row = 0;

    if (status != WF_OK) {
        platinum_preferences_set_status(
            &app->preferences,
            "Signed out locally; bridge revocation may have failed.");
        platinum_application_invalidate(app);
        return;
    }

    platinum_preferences_close(&app->preferences);
    SelectWindow(app->window);
    platinum_application_invalidate(app);
}

void platinum_application_recover_auth(platinum_application *app,
                                             wf_status status)
{
    if (app != NULL && status != WF_OK)
        app->last_error = status;
    if (app == NULL || status != WF_ERR_AUTH)
        return;

    /* Already unpaired: a stale window must not reopen the pairing dialog on
     * every refresh once the session is gone. */
    if (!platinum_session_is_paired(&app->session))
        return;

    /* The bridge token was refused, so it is gone or revoked. Drop the session
     * so the UI shows the unpaired state, clear what it was showing, and give
     * the user somewhere to go next instead of failing silently on every
     * subsequent refresh. Revocation is not attempted: the token is already
     * unusable, and there is nothing left to revoke server-side. */
    (void)platinum_session_sign_out(&app->session);
    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_timeline_init(&app->timeline);
    app->ui.selected_post = 0;
    app->ui.scroll_row = 0;

    (void)platinum_application_open_pairing(app);
    platinum_application_invalidate(app);
}

OSErr platinum_application_open_pairing(
    platinum_application *app)
{
    const platinum_config *config;
    const char *bridge_url;
    OSErr status;

    if (app == NULL)
        return paramErr;

    if (app->pairing.window != NULL) {
        SelectWindow(app->pairing.window);
        return noErr;
    }

    config = platinum_session_config(&app->session);
    bridge_url = config != NULL ? config->bridge_url : NULL;
    status = platinum_pairing_open(&app->pairing, bridge_url);
    if (status == noErr)
        SelectWindow(app->pairing.window);
    return status;
}

void platinum_application_show_pairing_error(
    platinum_application *app,
    const char *message)
{
    if (app == NULL || app->pairing.window == NULL)
        return;

    platinum_pairing_set_status(&app->pairing, message);
    SelectWindow(app->pairing.window);
}

/* The window for signing in with an app password. */
void platinum_application_open_apppw(platinum_application *app)
{
    const platinum_config *config;

    if (app == NULL)
        return;
    config = platinum_session_config(&app->session);
    if (platinum_apppw_open(&app->apppw,
                            config != NULL ? config->bridge_url : NULL) == noErr)
        SelectWindow(app->apppw.window);
}

/*
 * Sign in with the handle and password in the window. Plain http asks twice:
 * the first request only warns. However it ends, the password is wiped from the
 * entry buffer and from the local copy, and the window shows only bullets.
 */
void platinum_application_attempt_apppw(platinum_application *app)
{
    char bridge_url[PLATINUM_APPPW_URL_MAX + 1];
    char handle[PLATINUM_APPPW_HANDLE_MAX * 4 + 1];
    char password[PLATINUM_SECRET_MAX * 4 + 1];
    wf_status status;
    int reason;

    if (app == NULL || app->apppw.window == NULL)
        return;

    if (platinum_apppw_get_bridge_url(&app->apppw, bridge_url,
                                      sizeof(bridge_url)) != noErr) {
        platinum_apppw_set_status(&app->apppw, "Enter a bridge URL.");
        return;
    }
    if (platinum_apppw_get_handle(&app->apppw, handle, sizeof(handle)) != noErr) {
        platinum_apppw_set_status(&app->apppw, "Enter your handle.");
        return;
    }
    if (platinum_secret_utf8(&app->apppw.password, password,
                             sizeof(password)) < 0) {
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(&app->apppw, "Enter the app password.");
        return;
    }

    if (platinum_apppw_is_plain_http(bridge_url) && !app->apppw.http_armed) {
        app->apppw.http_armed = 1;
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(
            &app->apppw,
            "That address is plain http. Choose Sign In again to send it anyway.");
        return;
    }

    if (platinum_session_set_bridge_url(&app->session, bridge_url) != noErr) {
        platinum_bridge_wipe(password, sizeof(password));
        platinum_apppw_set_status(&app->apppw,
                                  "The bridge URL could not be saved.");
        return;
    }

    reason = PLATINUM_LOGIN_OTHER;
    status = platinum_session_sign_in_app_password(&app->session, handle,
                                                   password, &reason);
    platinum_bridge_wipe(password, sizeof(password));
    platinum_secret_wipe(&app->apppw.password);
    app->apppw.http_armed = 0;

    if (status != WF_OK) {
        platinum_apppw_set_status(
            &app->apppw,
            reason == PLATINUM_LOGIN_DISABLED
                ? "This bridge does not allow app-password sign-in."
            : reason == PLATINUM_LOGIN_INVALID
                ? "The handle or app password is not valid."
            : reason == PLATINUM_LOGIN_TOO_MANY
                ? "Too many failed attempts. Try again later."
            : reason == PLATINUM_LOGIN_BAD_SERVICE
                ? "The bridge does not accept that account service."
                : "Sign-in failed. Check the bridge address and try again.");
        return;
    }

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_pairing_close(&app->pairing);
    platinum_timeline_init(&app->timeline);
    platinum_apppw_close(&app->apppw);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}

void platinum_application_attempt_pair(
    platinum_application *app)
{
    char bridge_url[PLATINUM_PAIRING_URL_MAX + 1];
    char code[PLATINUM_PAIRING_CODE_MAX + 1];
    wf_status status;

    if (app == NULL || app->pairing.window == NULL)
        return;

    if (platinum_pairing_get_bridge_url(&app->pairing,
                                        bridge_url,
                                        sizeof(bridge_url)) != noErr) {
        platinum_application_show_pairing_error(
            app, "Enter a bridge URL.");
        return;
    }

    if (platinum_pairing_get_code(&app->pairing,
                                  code,
                                  sizeof(code)) != noErr) {
        platinum_application_show_pairing_error(
            app, "Enter a six-character pairing code.");
        return;
    }

    if (platinum_session_set_bridge_url(&app->session,
                                        bridge_url) != noErr) {
        platinum_application_show_pairing_error(
            app, "The bridge URL could not be saved.");
        return;
    }

    status = platinum_session_pair(&app->session, code);
    if (status != WF_OK) {
        platinum_application_show_pairing_error(
            app, "Pairing failed. Check the bridge and code.");
        return;
    }

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_timeline_init(&app->timeline);
    platinum_pairing_close(&app->pairing);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}
