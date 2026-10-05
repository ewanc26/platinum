#include "timeline.h"

static const platinum_post_preview kPosts[PLATINUM_TIMELINE_POSTS] = {
    {
        "Ewan Croft",
        "@ewancroft.uk",
        "just now",
        "Building a Bluesky client for a Macintosh that remembers what a Mac app is.",
        "",
        ""
    },
    {
        "Alice Example",
        "@alice.example",
        "5 minutes ago",
        "The new interface feels better when it uses the system controls instead of redrawing them.",
        "",
        ""
    },
    {
        "Robin Tester",
        "@robin.example",
        "20 minutes ago",
        "Classic Mac software works best when it treats the environment as a first-class platform.",
        "",
        ""
    },
    {
        "Morgan Sample",
        "@morgan.example",
        "1 hour ago",
        "The best retro apps do not try to pretend the platform isn't relevant.",
        "",
        ""
    },
    {
        "Dev Note",
        "@dev.example",
        "2 hours ago",
        "Keeping the network layer on the bridge means the Mac client can stay small.",
        "",
        ""
    }
};

const platinum_post_preview *platinum_timeline_posts(void)
{
    return kPosts;
}

unsigned short platinum_timeline_post_count(void)
{
    return PLATINUM_TIMELINE_POSTS;
}
