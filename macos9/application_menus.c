/* The menu bar: building it and acting on a choice. */

#include "application_internal.h"

static unsigned char kFileMenu[] = { 4, 'F', 'i', 'l', 'e' };

static unsigned char kEditMenu[] = { 4, 'E', 'd', 'i', 't' };

static unsigned char kViewMenu[] = { 4, 'V', 'i', 'e', 'w' };

static unsigned char kPostMenu[] = { 4, 'P', 'o', 's', 't' };

static unsigned char kLikeItem[] = {
    14, 'L', 'i', 'k', 'e', ' ', 'o', 'r', ' ', 'U', 'n', 'l', 'i', 'k', 'e'
};

static unsigned char kAuthorItem[] = {
    21, 'S', 'h', 'o', 'w', ' ', 'A', 'u', 't', 'h', 'o', 'r', '\'', 's', ' ',
    'P', 'r', 'o', 'f', 'i', 'l', 'e'
};

static unsigned char kFollowItem[] = {
    18, 'F', 'o', 'l', 'l', 'o', 'w', ' ', 'o', 'r', ' ', 'U', 'n', 'f', 'o', 'l', 'l', 'o', 'w'
};

static unsigned char kWordsItem[] = {
    11, 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd', 's'
};

static unsigned char kAddWordItem[] = {
    15, 'A', 'd', 'd', ' ', 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd'
};

static unsigned char kRemoveWordItem[] = {
    18, 'R', 'e', 'm', 'o', 'v', 'e', ' ', 'M', 'u', 't', 'e', 'd', ' ', 'W', 'o', 'r', 'd'
};

static unsigned char kQuoteItem[] = {
    13, 'Q', 'u', 'o', 't', 'e', ' ', 'P', 'o', 's', 't', '.', '.', '.'
};

static unsigned char kDeleteItem[] = {
    14, 'D', 'e', 'l', 'e', 't', 'e', ' ', 'M', 'y', ' ', 'P', 'o', 's', 't'
};

static unsigned char kMuteItem[] = {
    14, 'M', 'u', 't', 'e', ' ', 'o', 'r', ' ', 'U', 'n', 'm', 'u', 't', 'e'
};

static unsigned char kBlockItem[] = {
    16, 'B', 'l', 'o', 'c', 'k', ' ', 'o', 'r', ' ', 'U', 'n', 'b', 'l', 'o', 'c', 'k'
};

static unsigned char kAuthorPostsItem[] = {
    19, 'S', 'h', 'o', 'w', ' ', 'A', 'u', 't', 'h', 'o', 'r', '\'', 's', ' ',
    'P', 'o', 's', 't', 's'
};

static unsigned char kFeedsItem[] = {
    11, 'S', 'a', 'v', 'e', 'd', ' ', 'F', 'e', 'e', 'd', 's'
};

static unsigned char kListsItem[] = {
    8, 'M', 'y', ' ', 'L', 'i', 's', 't', 's'
};

static unsigned char kLikersItem[] = {
    14, 'W', 'h', 'o', ' ', 'L', 'i', 'k', 'e', 'd', ' ', 'T', 'h', 'i', 's'
};

static unsigned char kRepostersItem[] = {
    17, 'W', 'h', 'o', ' ', 'R', 'e', 'p', 'o', 's', 't', 'e', 'd', ' ', 'T', 'h', 'i', 's'
};

static unsigned char kFollowersItem[] = {
    14, 'S', 'h', 'o', 'w', ' ', 'F', 'o', 'l', 'l', 'o', 'w', 'e', 'r', 's'
};

static unsigned char kFollowingItem[] = {
    14, 'S', 'h', 'o', 'w', ' ', 'F', 'o', 'l', 'l', 'o', 'w', 'i', 'n', 'g'
};

static unsigned char kReplyItem[] = {
    8, 'R', 'e', 'p', 'l', 'y', '.', '.', '.'
};

static unsigned char kRepostItem[] = {
    21, 'R', 'e', 'p', 'o', 's', 't', ' ', 'o', 'r', ' ', 'U', 'n', 'd', 'o',
    ' ', 'R', 'e', 'p', 'o', 's', 't'
};

static unsigned char kWindowMenu[] = { 6, 'W', 'i', 'n', 'd', 'o', 'w' };

static unsigned char kHelpMenu[] = { 4, 'H', 'e', 'l', 'p' };


static unsigned char kPairAccount[] = {
    15, 'P', 'a', 'i', 'r', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', '.', '.', '.'
};

static unsigned char kNewPost[] = {
    11, 'N', 'e', 'w', ' ', 'P', 'o', 's', 't', '.', '.', '.'
};

static unsigned char kAppPassword[] = {
    29, 'S', 'i', 'g', 'n', ' ', 'I', 'n', ' ', 'w', 'i', 't', 'h', ' ', 'A',
    'p', 'p', ' ', 'P', 'a', 's', 's', 'w', 'o', 'r', 'd', '.', '.', '.'
};

static unsigned char kCloseWindow[] = {
    12, 'C', 'l', 'o', 's', 'e', ' ', 'W', 'i', 'n', 'd', 'o', 'w'
};

static unsigned char kQuit[] = { 4, 'Q', 'u', 'i', 't' };

static unsigned char kUndo[] = { 4, 'U', 'n', 'd', 'o' };

static unsigned char kCut[] = { 3, 'C', 'u', 't' };

static unsigned char kCopy[] = { 4, 'C', 'o', 'p', 'y' };

static unsigned char kPaste[] = { 5, 'P', 'a', 's', 't', 'e' };

static unsigned char kSelectAll[] = {
    10, 'S', 'e', 'l', 'e', 'c', 't', ' ', 'A', 'l', 'l'
};

static unsigned char kPreferences[] = {
    14, 'P', 'r', 'e', 'f', 'e', 'r', 'e', 'n', 'c', 'e', 's', '.', '.', '.'
};

static unsigned char kRefreshMenu[] = { 7, 'R', 'e', 'f', 'r', 'e', 's', 'h' };

static unsigned char kLoadOlder[] = {
    16, 'L', 'o', 'a', 'd', ' ', 'O', 'l', 'd', 'e', 'r', ' ', 'P', 'o', 's',
    't', 's'
};

static unsigned char kSearchAccounts[] = {
    18, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'A', 'c', 'c', 'o', 'u', 'n', 't', 's', '.', '.', '.'
};

static unsigned char kSearchPosts[] = {
    15, 'S', 'e', 'a', 'r', 'c', 'h', ' ', 'P', 'o', 's', 't', 's', '.', '.', '.'
};

static unsigned char kShowThread[] = {
    11, 'S', 'h', 'o', 'w', ' ', 'T', 'h', 'r', 'e', 'a', 'd'
};

static unsigned char kShowDetail[] = {
    11, 'S', 'h', 'o', 'w', ' ', 'D', 'e', 't', 'a', 'i', 'l'
};

static unsigned char kTimelineWindow[] = {
    8, 'T', 'i', 'm', 'e', 'l', 'i', 'n', 'e'
};

static unsigned char kNotificationsWindow[] = {
    13, 'N', 'o', 't', 'i', 'f', 'i', 'c', 'a', 't', 'i', 'o', 'n', 's'
};

static unsigned char kProfileWindow[] = {
    7, 'P', 'r', 'o', 'f', 'i', 'l', 'e'
};

static unsigned char kBringAllToFront[] = {
    18, 'B', 'r', 'i', 'n', 'g', ' ', 'A', 'l', 'l', ' ', 't', 'o', ' ', 'F', 'r', 'o', 'n', 't'
};

static unsigned char kAbout[] = {
    14, 'A', 'b', 'o', 'u', 't', ' ', 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm'
};

static unsigned char kStatusItem[] = {
    17, 'C', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n', ' ', 'S', 't', 'a',
    't', 'u', 's'
};

static unsigned char kHelpItem[] = {
    13, 'P', 'l', 'a', 't', 'i', 'n', 'u', 'm', ' ', 'H', 'e', 'l', 'p'
};

OSErr platinum_application_create_menus(platinum_application *app)
{
    if (app == NULL)
        return paramErr;

    app->file_menu = NewMenu(kFileMenuID, kFileMenu);
    app->edit_menu = NewMenu(kEditMenuID, kEditMenu);
    app->view_menu = NewMenu(kViewMenuID, kViewMenu);
    app->post_menu = NewMenu(kPostMenuID, kPostMenu);
    app->window_menu = NewMenu(kWindowMenuID, kWindowMenu);
    app->help_menu = NewMenu(kHelpMenuID, kHelpMenu);

    if (app->file_menu == NULL || app->edit_menu == NULL ||
        app->view_menu == NULL || app->post_menu == NULL ||
        app->window_menu == NULL ||
        app->help_menu == NULL) {
        platinum_application_dispose_menus(app);
        return memFullErr;
    }

    AppendMenu(app->file_menu, kNewPost);
    AppendMenu(app->file_menu, kPairAccount);
    AppendMenu(app->file_menu, kAppPassword);
    AppendMenu(app->file_menu, kCloseWindow);
    AppendMenu(app->file_menu, kQuit);
    SetItemCmd(app->file_menu, 1, 'n');
    SetItemCmd(app->file_menu, 2, 'k');
    SetItemCmd(app->file_menu, 4, 'w');
    SetItemCmd(app->file_menu, 5, 'q');

    AppendMenu(app->edit_menu, kUndo);
    AppendMenu(app->edit_menu, kCut);
    AppendMenu(app->edit_menu, kCopy);
    AppendMenu(app->edit_menu, kPaste);
    AppendMenu(app->edit_menu, kSelectAll);
    AppendMenu(app->edit_menu, kPreferences);
    SetItemCmd(app->edit_menu, 1, 'z');
    SetItemCmd(app->edit_menu, 2, 'x');
    SetItemCmd(app->edit_menu, 3, 'c');
    SetItemCmd(app->edit_menu, 4, 'v');
    SetItemCmd(app->edit_menu, 5, 'a');
    SetItemCmd(app->edit_menu, 6, ',');
    DisableItem(app->edit_menu, 1);
    DisableItem(app->edit_menu, 2);
    DisableItem(app->edit_menu, 3);
    DisableItem(app->edit_menu, 4);
    DisableItem(app->edit_menu, 5);

    AppendMenu(app->view_menu, kRefreshMenu);
    AppendMenu(app->view_menu, kShowDetail);
    AppendMenu(app->view_menu, kLoadOlder);
    AppendMenu(app->view_menu, kShowThread);
    SetItemCmd(app->view_menu, 4, 't');
    AppendMenu(app->view_menu, kFeedsItem);
    AppendMenu(app->view_menu, kListsItem);
    AppendMenu(app->view_menu, kSearchAccounts);
    AppendMenu(app->view_menu, kSearchPosts);
    SetItemCmd(app->view_menu, 8, 'f');
    AppendMenu(app->view_menu, kWordsItem);
    AppendMenu(app->view_menu, kAddWordItem);
    AppendMenu(app->view_menu, kRemoveWordItem);
    SetItemCmd(app->view_menu, 1, 'r');

    AppendMenu(app->post_menu, kLikeItem);
    AppendMenu(app->post_menu, kRepostItem);
    AppendMenu(app->post_menu, kReplyItem);
    AppendMenu(app->post_menu, kAuthorItem);
    AppendMenu(app->post_menu, kFollowItem);
    AppendMenu(app->post_menu, kLikersItem);
    AppendMenu(app->post_menu, kRepostersItem);
    AppendMenu(app->post_menu, kFollowersItem);
    AppendMenu(app->post_menu, kFollowingItem);
    AppendMenu(app->post_menu, kAuthorPostsItem);
    AppendMenu(app->post_menu, kMuteItem);
    AppendMenu(app->post_menu, kBlockItem);
    AppendMenu(app->post_menu, kQuoteItem);
    AppendMenu(app->post_menu, kDeleteItem);
    SetItemCmd(app->post_menu, 1, 'l');
    SetItemCmd(app->post_menu, 2, 'e');
    SetItemCmd(app->post_menu, 3, 'j');
    SetItemCmd(app->post_menu, 4, 'i');
    SetItemCmd(app->post_menu, 5, 'y');

    AppendMenu(app->window_menu, kTimelineWindow);
    AppendMenu(app->window_menu, kNotificationsWindow);
    AppendMenu(app->window_menu, kProfileWindow);
    AppendMenu(app->window_menu, kBringAllToFront);

    AppendMenu(app->help_menu, kAbout);
    AppendMenu(app->help_menu, kHelpItem);
    AppendMenu(app->help_menu, kStatusItem);

    InsertMenu(app->file_menu, 0);
    InsertMenu(app->edit_menu, 0);
    InsertMenu(app->view_menu, 0);
    InsertMenu(app->post_menu, 0);
    InsertMenu(app->window_menu, 0);
    InsertMenu(app->help_menu, 0);
    DrawMenuBar();

    return noErr;
}

void platinum_application_dispose_menus(platinum_application *app)
{
    if (app == NULL)
        return;

    if (app->help_menu != NULL) {
        DeleteMenu(kHelpMenuID);
        DisposeMenu(app->help_menu);
        app->help_menu = NULL;
    }
    if (app->window_menu != NULL) {
        DeleteMenu(kWindowMenuID);
        DisposeMenu(app->window_menu);
        app->window_menu = NULL;
    }
    if (app->post_menu != NULL) {
        DeleteMenu(kPostMenuID);
        DisposeMenu(app->post_menu);
        app->post_menu = NULL;
    }
    if (app->view_menu != NULL) {
        DeleteMenu(kViewMenuID);
        DisposeMenu(app->view_menu);
        app->view_menu = NULL;
    }
    if (app->edit_menu != NULL) {
        DeleteMenu(kEditMenuID);
        DisposeMenu(app->edit_menu);
        app->edit_menu = NULL;
    }
    if (app->file_menu != NULL) {
        DeleteMenu(kFileMenuID);
        DisposeMenu(app->file_menu);
        app->file_menu = NULL;
    }

    DrawMenuBar();
}

void platinum_application_handle_menu(platinum_application *app,
                                             long choice)
{
    short menu_id;
    short item;

    if (app == NULL || choice == 0)
        return;

    menu_id = (short)((choice >> 16) & 0xFFFF);
    item = (short)(choice & 0xFFFF);

    if (!(menu_id == kPostMenuID && item == 12))
        app->profile.block_armed = 0;
    if (!(menu_id == kPostMenuID && item == 14))
        app->delete_armed = 0;

    if (menu_id == kFileMenuID) {
        if (item == 1) {
            platinum_application_new_post(app);
        } else if (item == 2) {
            if (platinum_application_open_pairing(app) == noErr)
                SelectWindow(app->pairing.window);
        } else if (item == 3) {
            platinum_application_open_apppw(app);
        } else if (item == 4) {
            if (app->apppw.window != NULL)
                platinum_apppw_close(&app->apppw);
            else if (app->pairing.window != NULL)
                platinum_pairing_close(&app->pairing);
            else if (app->preferences.window != NULL)
                platinum_preferences_close(&app->preferences);
            else if (app->compose.window != NULL)
                platinum_application_close_compose(app, 0);
            else
                app->running = 0;
        } else if (item == 5) {
            app->running = 0;
        }
    } else if (menu_id == kEditMenuID) {
        if (item == 6)
            platinum_application_open_preferences(app);
    } else if (menu_id == kViewMenuID) {
        if (item == 1) {
            platinum_application_refresh_timeline(app);
        } else if (item == 2) {
            app->ui.show_detail = !app->ui.show_detail;
            platinum_application_invalidate(app);
        } else if (item == 3) {
            platinum_application_load_older(app);
        } else if (item == 4) {
            platinum_application_show_thread(app);
        } else if (item == 5) {
            platinum_application_show_named(app, PLATINUM_PEOPLE_FEEDS);
        } else if (item == 6) {
            platinum_application_show_named(app, PLATINUM_PEOPLE_LISTS);
        } else if (item == 9) {
            platinum_application_show_words(app);
        } else if (item == 10) {
            if (platinum_search_open(&app->search, PLATINUM_SEARCH_WORD) == noErr)
                SelectWindow(app->search.window);
        } else if (item == 11) {
            platinum_application_remove_word(app);
        } else if (item == 7 || item == 8) {
            if (platinum_search_open(&app->search, item == 8 ? PLATINUM_SEARCH_POSTS : PLATINUM_SEARCH_ACCOUNTS) == noErr)
                SelectWindow(app->search.window);
        }
    } else if (menu_id == kPostMenuID) {
        if (item == 14)
            platinum_application_delete_post(app);
        else if (item == 13)
            platinum_application_quote(app);
        else if (item == 12)
            platinum_application_relate(app, PLATINUM_RELATION_BLOCK);
        else if (item == 11)
            platinum_application_relate(app, PLATINUM_RELATION_MUTE);
        else if (item == 10)
            platinum_application_show_posts(app);
        else if (item >= 6 && item <= 9)
            platinum_application_show_people(app, item);
        else if (item == 4)
            platinum_application_show_author(app);
        else if (item == 5)
            platinum_application_follow(app);
        else if (item == 3)
            platinum_application_reply(app);
        else
            platinum_application_engage(app, item == 2);
    } else if (menu_id == kWindowMenuID) {
        if (item == 2)
            platinum_application_open_notifications(app);
        else if (item == 3)
            platinum_application_open_profile(app);
        else
            platinum_application_invalidate(app);
    } else if (menu_id == kHelpMenuID) {
        if (item == 3)
            platinum_application_open_diag(app);
        platinum_application_invalidate(app);
    }
}
