/* Composing, replying, quoting, engaging and deleting. */

#include "application_internal.h"

/* Open a compose window that answers the selected post. */
void platinum_application_reply(platinum_application *app)
{
    const platinum_post_preview *post;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    if (app->compose.window != NULL) {
        SelectWindow(app->compose.window);
        return;
    }

    post = &app->timeline.posts[index];
    if (platinum_compose_open(&app->compose) != noErr)
        return;
    (void)platinum_compose_set_reply(&app->compose, post->uri, post->cid,
                                     post->handle);
    SelectWindow(app->compose.window);
    platinum_application_invalidate(app);
}

/*
 * Delete the selected post if it is yours. Deleting cannot be undone, so the
 * first request only asks; the same request again does it, and any other menu
 * choice cancels. The bridge checks ownership too.
 */
void platinum_application_delete_post(platinum_application *app)
{
    platinum_bridge_client *bridge;
    const platinum_config *config;
    const platinum_post_preview *post;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    bridge = platinum_session_bridge(&app->session);
    config = platinum_session_config(&app->session);
    if (bridge == NULL || config == NULL)
        return;

    post = &app->timeline.posts[index];
    if (!platinum_post_uri_is_in_repo(post->uri, config->did)) {
        app->delete_armed = 0;
        strcpy(app->timeline.status, "That is not your post, so I can't delete it.");
        platinum_application_invalidate(app);
        return;
    }
    if (!app->delete_armed) {
        app->delete_armed = 1;
        strcpy(app->timeline.status,
               "Choose Delete My Post again to delete this post for good. Any other menu choice cancels.");
        platinum_application_invalidate(app);
        return;
    }
    app->delete_armed = 0;

    status = platinum_bridge_delete_post(bridge, post->uri);
    platinum_application_recover_auth(app, status);
    if (status != WF_OK) {
        strcpy(app->timeline.status, "The post could not be deleted.");
        platinum_application_invalidate(app);
        return;
    }
    platinum_application_refresh_timeline(app);
    platinum_application_invalidate(app);
}

/*
 * A new, plain post: opens compose (or brings it forward) and puts back the
 * text that was left unsent last time, if any.
 */
void platinum_application_new_post(platinum_application *app)
{
    char draft[PLATINUM_DRAFT_MAX + 1];

    if (app == NULL)
        return;
    if (app->compose.window != NULL) {
        SelectWindow(app->compose.window);
        return;
    }
    if (platinum_compose_open(&app->compose) != noErr)
        return;
    if (platinum_draft_load(draft, sizeof(draft)) == noErr && draft[0] != '\0') {
        platinum_compose_set_text(&app->compose, draft);
        platinum_compose_set_status(&app->compose,
                                    "Restored the post you did not send.");
    }
    SelectWindow(app->compose.window);
}

/*
 * Close the compose window. A new post that was not sent keeps its text as a
 * draft (empty text clears the draft); a sent post clears it. A reply or a quote
 * is not kept, so it can never come back as a plain post.
 */
void platinum_application_close_compose(platinum_application *app,
                                               int sent)
{
    char text[PLATINUM_COMPOSE_MAX_TEXT + 1];

    if (app == NULL)
        return;
    if (app->compose.window != NULL) {
        if (sent) {
            (void)platinum_draft_clear();
        } else if (app->compose.reply_uri[0] == '\0' &&
                   app->compose.quote_uri[0] == '\0') {
            if (platinum_compose_get_text(&app->compose, text,
                                          sizeof(text)) == noErr)
                (void)platinum_draft_save(text);
            else
                (void)platinum_draft_clear();
        }
    }
    platinum_compose_close(&app->compose);
}

/* Open compose as a quote of the selected post. */
void platinum_application_quote(platinum_application *app)
{
    const platinum_post_preview *post;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    if (app->compose.window != NULL) {
        SelectWindow(app->compose.window);
        return;
    }

    post = &app->timeline.posts[index];
    if (platinum_compose_open(&app->compose) != noErr)
        return;
    (void)platinum_compose_set_quote(&app->compose, post->uri, post->cid,
                                     post->handle);
    SelectWindow(app->compose.window);
    platinum_application_invalidate(app);
}

/* Like or repost the selected post, or undo it if the row says it is done. */
void platinum_application_engage(platinum_application *app, int repost)
{
    platinum_bridge_client *bridge;
    const platinum_post_preview *post;
    wf_status status;
    short index;

    if (app == NULL || !platinum_session_is_paired(&app->session))
        return;
    index = app->ui.selected_post;
    if (index < 0 || index >= (short)app->timeline.count)
        return;
    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL)
        return;

    post = &app->timeline.posts[index];
    status = platinum_timeline_set_engagement(
        &app->timeline, bridge, (unsigned short)index, repost,
        repost ? !post->reposted : !post->liked);
    platinum_application_recover_auth(app, status);
    platinum_application_invalidate(app);
}

void platinum_application_post_status(platinum_application *app,
                                             wf_status status)
{
    if (app != NULL && status != WF_OK)
        app->last_error = status;
    if (app == NULL || app->compose.window == NULL)
        return;

    if (status == WF_ERR_AUTH)
        platinum_compose_set_status(&app->compose,
                                    "Session expired. Use File > Pair Account; your draft is preserved.");
    else if (status == WF_ERR_HTTP)
        platinum_compose_set_status(&app->compose,
                                    "The bridge rejected the post.");
    else if (status == WF_ERR_NETWORK || status == WF_ERR_TIMEOUT)
        platinum_compose_set_status(&app->compose,
                                    "The bridge could not be reached.");
    else
        platinum_compose_set_status(&app->compose,
                                    "The post could not be sent.");
}

void platinum_application_submit_post(platinum_application *app)
{
    char text[PLATINUM_COMPOSE_MAX_TEXT + 1];
    char utf8_text[PLATINUM_TEXT_UTF8_CAPACITY];
    char *body;
    wf_response response;
    wf_status status;
    platinum_bridge_client *bridge;
    int gate_failed;

    if (app == NULL || app->compose.window == NULL)
        return;

    if (!platinum_session_is_paired(&app->session)) {
        platinum_compose_set_status(&app->compose,
                                    "Pair an account before posting.");
        return;
    }

    if (platinum_compose_get_text(&app->compose,
                                  text,
                                  sizeof(text)) != noErr) {
        platinum_compose_set_status(&app->compose,
                                    "Enter up to 300 MacRoman characters.");
        return;
    }

    if (platinum_text_macroman_to_utf8(text,
                                       utf8_text,
                                       sizeof(utf8_text),
                                       NULL) < 0) {
        platinum_compose_set_status(&app->compose,
                                    "The post text could not be encoded.");
        return;
    }

    body = platinum_bridge_post_body_ex(utf8_text,
                                        app->compose.reply_uri,
                                        app->compose.reply_cid,
                                        app->compose.quote_uri,
                                        app->compose.quote_cid,
                                        app->compose.reply_gate);
    if (body == NULL) {
        platinum_compose_set_status(&app->compose,
                                    "Not enough memory to prepare the post.");
        return;
    }

    bridge = platinum_session_bridge(&app->session);
    if (bridge == NULL) {
        free(body);
        platinum_compose_set_status(&app->compose,
                                    "The bridge session is unavailable.");
        return;
    }

    platinum_compose_set_posting(&app->compose, 1);
    platinum_application_invalidate(app);
    memset(&response, 0, sizeof(response));

    status = platinum_bridge_post(bridge, "/v1/post", body, &response);
    free(body);
    gate_failed = status == WF_OK && response.body != NULL &&
                  platinum_bridge_reply_gate_failed(response.body);
    wf_response_free(&response);
    platinum_compose_set_posting(&app->compose, 0);

    if (status != WF_OK) {
        platinum_application_post_status(app, status);
        return;
    }

    platinum_compose_set_status(&app->compose, NULL);
    platinum_application_close_compose(app, 1);
    SelectWindow(app->window);
    platinum_application_refresh_timeline(app);
    if (gate_failed) {
        /* The post is out; say plainly that the limit was not set. */
        strcpy(app->timeline.status,
               "Posted, but I could not limit who can reply. Anyone can.");
    }
    platinum_application_invalidate(app);
}
