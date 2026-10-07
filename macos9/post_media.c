#include <string.h>

#include "post_media.h"
#include "text_codec.h"

/* A string member of `object` into `destination`, converted to MacRoman and
 * clipped to `capacity`. Leaves "" and returns 0 if it is absent. */
static int media_copy(char *destination,
                      long capacity,
                      platinum_json object,
                      const char *name)
{
    char utf8[PLATINUM_TEXT_UTF8_CAPACITY];

    destination[0] = '\0';
    if (platinum_json_string_truncating(object, name, utf8, sizeof(utf8)) !=
        WF_OK)
        return 0;
    return platinum_text_utf8_to_macroman(utf8, destination, capacity, NULL) >=
           0;
}

/* A reference is an identifier, not display text: refuse one that does not fit
 * rather than send a cut-off one back to the bridge. */
static int media_ref(char *destination, platinum_json object, const char *name)
{
    destination[0] = '\0';
    if (platinum_json_string(object, name, destination,
                             PLATINUM_MEDIA_REF_MAX) != WF_OK) {
        destination[0] = '\0';
        return 0;
    }
    return destination[0] != '\0';
}

void platinum_post_media_parse(platinum_post_media *media,
                               platinum_json author,
                               platinum_json item)
{
    platinum_json images;
    platinum_json card;
    long count = 0;
    long i;

    if (media == NULL)
        return;
    memset(media, 0, sizeof(*media));

    (void)media_ref(media->avatar, author, "avatar");

    if (platinum_json_member(item, "images", &images) == WF_OK &&
        platinum_json_count(images, &count) == WF_OK) {
        for (i = 0; i < count && media->image_count < PLATINUM_MEDIA_IMAGES_MAX;
             ++i) {
            platinum_json element;
            platinum_media_image *slot = &media->images[media->image_count];

            if (platinum_json_element(images, i, &element) != WF_OK)
                continue;
            if (!media_ref(slot->ref, element, "ref"))
                continue;
            (void)media_copy(slot->alt, sizeof(slot->alt), element, "alt");
            media->image_count++;
        }
    }

    if (platinum_json_member(item, "card", &card) == WF_OK) {
        (void)media_copy(media->card_title, sizeof(media->card_title), card,
                         "title");
        (void)media_copy(media->card_domain, sizeof(media->card_domain), card,
                         "domain");
    }
}
