#include "application.h"

int main(void)
{
    platinum_application app;
    OSErr err;

    err = platinum_application_init(&app);
    if (err != noErr)
        return (int)err;

    platinum_application_run(&app);
    platinum_application_dispose(&app);
    return 0;
}
