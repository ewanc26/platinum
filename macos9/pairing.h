#ifndef PLATINUM_PAIRING_H
#define PLATINUM_PAIRING_H

#include <Events.h>
#include <TextEdit.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLATINUM_PAIRING_URL_MAX 255
#define PLATINUM_PAIRING_CODE_MAX 6
#define PLATINUM_PAIRING_STATUS_MAX 159

typedef struct platinum_pairing {
    WindowPtr window;
    TEHandle bridge_url;
    TEHandle code;
    short active_field;
    char status[PLATINUM_PAIRING_STATUS_MAX + 1];
} platinum_pairing;

enum {
    PLATINUM_PAIRING_NONE = 0,
    PLATINUM_PAIRING_PAIR = 1,
    PLATINUM_PAIRING_CANCEL = 2
};

OSErr platinum_pairing_open(platinum_pairing *pairing,
                            const char *bridge_url);
void platinum_pairing_close(platinum_pairing *pairing);
int platinum_pairing_handle_event(platinum_pairing *pairing,
                                   EventRecord *event);
void platinum_pairing_draw(platinum_pairing *pairing);

OSErr platinum_pairing_get_bridge_url(const platinum_pairing *pairing,
                                      char *buffer,
                                      long capacity);
OSErr platinum_pairing_get_code(const platinum_pairing *pairing,
                                char *buffer,
                                long capacity);
void platinum_pairing_set_status(platinum_pairing *pairing,
                                 const char *status);

#ifdef __cplusplus
}
#endif

#endif
