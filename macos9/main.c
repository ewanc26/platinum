#include "application.h"

int main(void)
{
    /* Static, not automatic: the application state is tens of kilobytes (the
     * timeline alone is 20+ post previews) and a Classic Mac application's
     * stack is small and fixed when it launches. Static storage lives in the
     * application's data, which the Memory Manager sized from the binary. */
    static platinum_application app;
    OSErr err;

    err = platinum_application_init(&app);
    if (err != noErr)
        return (int)err;

    platinum_application_run(&app);
    platinum_application_dispose(&app);
    return 0;
}
