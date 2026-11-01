/* Signing in natively with a handle and an app password, the bridge's own
 * pairing for the screens it still serves, signing out, and recovering from a
 * lost session. */

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
    int native;

    if (app != NULL && status != WF_OK)
        app->last_error = status;
    if (app == NULL || status != WF_ERR_AUTH)
        return;

    /* Already unpaired: a stale window must not reopen the pairing dialog on
     * every refresh once the session is gone. */
    if (!platinum_session_is_paired(&app->session))
        return;

    /* The session was refused, so it is gone or revoked. Drop it so the UI
     * shows the signed-out state, clear what it was showing, and give the user
     * somewhere to go next instead of failing silently on every subsequent
     * refresh. Revocation is not attempted: the credential is already unusable,
     * and there is nothing left to revoke server-side. */
    native = platinum_session_client(&app->session) != NULL;
    (void)platinum_session_sign_out(&app->session);
    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_timeline_init(&app->timeline);
    app->ui.selected_post = 0;
    app->ui.scroll_row = 0;

    if (native)
        (void)platinum_application_open_apppw(app);
    else
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

/* The window for signing in to the account's own PDS. */
void platinum_application_open_apppw(platinum_application *app)
{
    const platinum_atproto_session *account;

    if (app == NULL)
        return;
    account = platinum_session_native_account(&app->session);
    if (platinum_apppw_open(&app->apppw,
                            account != NULL ? account->handle : NULL) == noErr)
        SelectWindow(app->apppw.window);
}

static const char *apppw_failure_text(wf_status status)
{
    if (status == WF_ERR_AUTH)
        return "The handle or app password is not valid.";
    if (status == WF_ERR_HANDLE_DOCUMENT_NOT_FOUND ||
        status == WF_ERR_HANDLE_RESOLVE)
        return "That handle could not be resolved to an account.";
    if (status == WF_ERR_DID_DOCUMENT_NOT_FOUND)
        return "The account's identity document could not be read.";
    if (status == WF_ERR_NETWORK || status == WF_ERR_TIMEOUT)
        return "The server could not be reached. Check the connection.";
    if (status == WF_ERR_CRYPTO)
        return "The secure connection to the server failed.";
    if (status == WF_ERR_RATE_LIMIT)
        return "Too many attempts. Try again later.";
    if (status == WF_ERR_PARSE)
        return "The server's answer could not be understood.";
    if (status == WF_ERR_INVALID_ARG)
        return "Enter a handle like you.bsky.social.";
    return "Sign-in failed.";
}

/*
 * Find the account from the handle and sign in to its own PDS. However it ends,
 * the password is wiped from the entry buffer and from the local copy, and the
 * window shows only bullets. On success the window stays open as the signed-in
 * view: the handle, the PDS and which handle method found the account.
 */
void platinum_application_attempt_apppw(platinum_application *app)
{
    char handle[PLATINUM_APPPW_HANDLE_MAX * 4 + 1];
    char password[PLATINUM_SECRET_MAX * 4 + 1];
    const platinum_atproto_session *account;
    wf_status status;

    if (app == NULL || app->apppw.window == NULL)
        return;

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

    platinum_apppw_set_resolving(&app->apppw, 1);
    status = platinum_session_sign_in_native(&app->session, handle, password);
    platinum_bridge_wipe(password, sizeof(password));
    platinum_secret_wipe(&app->apppw.password);
    platinum_apppw_set_resolving(&app->apppw, 0);

    if (status != WF_OK) {
        platinum_apppw_set_status(&app->apppw, apppw_failure_text(status));
        return;
    }

    account = platinum_session_native_account(&app->session);
    if (account != NULL) {
        char shown[PLATINUM_APPPW_RESOLVED_HANDLE_MAX + 2];

        shown[0] = '@';
        strncpy(shown + 1, account->handle, sizeof(shown) - 2);
        shown[sizeof(shown) - 1] = '\0';
        platinum_apppw_set_resolved(
            &app->apppw, shown, account->pds_url,
            platinum_session_entryway_name(
                platinum_session_entryway(&app->session)));
    }

    platinum_profile_close(&app->profile);
    platinum_notifications_close(&app->notifications);
    platinum_thread_close(&app->thread);
    platinum_people_close(&app->people);
    platinum_search_close(&app->search);
    platinum_pairing_close(&app->pairing);
    platinum_timeline_init(&app->timeline);
    if (account != NULL) {
        /* The signed-in view stays in front; closing it returns to the main
         * window. */
        SelectWindow(app->apppw.window);
    } else {
        platinum_apppw_close(&app->apppw);
        SelectWindow(app->window);
    }
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
