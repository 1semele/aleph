#include <aleph/defs.h>
#include <aleph/audio.h>
#include <aleph/gui.h>

int main() {
    LOG("aleph v0.1");

    audio_init();
    gui_init();

    while (gui_is_running()) {
        gui_update();
        gui_draw();
    }

    audio_stop();
    return EXIT_SUCCESS;
}