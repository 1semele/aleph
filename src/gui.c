#include <SDL.h>

#include <GL/glew.h>
#include <GL/gl.h>

#include <aleph/shader.h>
#include <aleph/audio.h>
#include <aleph/gui.h>

#define BUTTON_LEFT 0 
#define BUTTON_RIGHT 1
#define BUTTON_MIDDLE 2

typedef enum {
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    KEY_SPACE,

    KEY_SHIFT,
    KEY_ESC,

    _KEY_MAX,
} Key;

#define MAX(_x, _y) ((_x) > (_y) ? (_x) : (_y))

typedef enum {
    SLICE_EMPTY,
    SLICE_FIRST_MARK,
    SLICE_FINISHED,
} Slice_State;

typedef struct {
    Slice_Id id;
    Slice_State state;
    size_t start, end;
    bool loop;
    bool pressed;
} Slice;

#define MAX_SLICES 10

struct {
    SDL_Window *win;
    int win_w, win_h;
    bool running;
    Shader shader;

    size_t zoom, start;
    size_t old_zoom, old_start;
    float down_x, down_y;

    bool button_down[3];
    bool button_pressed[3];
    bool button_released[3];
    float mouse_x, mouse_y;

    Slice slices[MAX_SLICES];
    int active_slice;

    size_t cursor_index;

    bool key_pressed[_KEY_MAX];
    bool key_released[_KEY_MAX];
    bool key_down[_KEY_MAX];

} gui;

typedef struct {
    float x, y, z;
} Vec3;

Vec3 cursor_color = {0.72156862745, 0.72156862745, 0.56078431372};
Vec3 slice_color = {1.0f, 1.0f, 1.0f};

static int sdl_button_to_num(int button);
static int sdl_key_to_num(int key);
static void draw_marker(size_t index, Vec3 color);
static void gui_get_input();

void gui_init() {
    gui.win_w = 960;
    gui.win_h = 540;

    gui.running = true;

    gui.active_slice = 0;
    for (int i = 0; i < MAX_SLICES; i++) {
        gui.slices[0].state = SLICE_EMPTY;
    }

    SDL_Init(SDL_INIT_VIDEO);

    gui.win = SDL_CreateWindow("aleph", SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED, 960, 540, SDL_WINDOW_OPENGL);
    SDL_GLContext gl = SDL_GL_CreateContext(gui.win);
    SDL_GL_MakeCurrent(gui.win, gl);
    if (glewInit() != GLEW_OK) {
        FAIL("glew error");
    }

    SDL_GL_SetSwapInterval(1);

    gui.start = 0;
    gui.zoom = 256;
}

bool gui_is_running() {
    return gui.running;
}

void gui_update() {
    gui_get_input();

    if (gui.button_pressed[BUTTON_RIGHT]) {
        gui.down_x = gui.mouse_x;
        gui.down_y = gui.mouse_y;
        if (gui.key_down[KEY_SHIFT]) {
            gui.old_start = gui.start;
        } else {
            gui.old_zoom = gui.zoom;
        }
    } 

    if (gui.button_down[BUTTON_RIGHT]) {
        float diff_x = gui.down_x - gui.mouse_x;
        float diff_y = gui.down_y - gui.mouse_y;
        if (gui.key_down[KEY_SHIFT]) {
            gui.start = MAX(0.0f, gui.old_start + diff_x);
        } else {
            gui.zoom = MAX(1.0f, gui.old_zoom + diff_y);
        }
    }

    if (gui.button_down[BUTTON_LEFT]) {
        float pixels_from_start = gui.mouse_x - 100.0f;
        float jump_index = (pixels_from_start + gui.start) * gui.zoom;
        gui.cursor_index = jump_index;
    }

    if (gui.key_pressed[KEY_SPACE]) {
        Slice *slice = &gui.slices[gui.active_slice];
        switch (slice->state) {
            case SLICE_EMPTY:
                slice->state = SLICE_FIRST_MARK;
                slice->start = gui.cursor_index;
                break;
            case SLICE_FIRST_MARK:
                slice->state = SLICE_FINISHED;
                slice->end = gui.cursor_index;
                if (slice->start > slice->end) {
                    slice->end = slice->start;
                    slice->start = gui.cursor_index;
                }
                slice->id = audio_slice_begin(slice->start, slice->end, false);
                break;
            case SLICE_FINISHED:
                break;
        }
    }

    for (int i = 0; i < 9; i++) {
        if (gui.key_pressed[KEY_1 + i]) {
            if (gui.key_down[KEY_SHIFT]) {
                gui.active_slice = i;
            } else {
                Slice *slice = &gui.slices[i];
                audio_slice_set_index(slice->id, 0);
                audio_slice_play(slice->id);
                slice->pressed = true;
            }
        }

        if (gui.key_released[KEY_1 + i]) {
            Slice *slice = &gui.slices[i];
            if (gui.slices[i].pressed) {
                audio_slice_stop(slice->id);
            }
        }
    }

    if (gui.key_pressed[KEY_ESC]) {
        Slice *slice = &gui.slices[gui.active_slice];
        switch (slice->state) {
            case SLICE_EMPTY:
                break;
            case SLICE_FIRST_MARK:
                slice->state = SLICE_EMPTY;
                break;
            case SLICE_FINISHED:
                audio_slice_end(slice->id);
                audio_slice_stop(slice->id);
                slice->state = SLICE_EMPTY;
                break;
        }
    }
}

void gui_draw() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBegin(GL_LINES);

    size_t len = 0;
    float *data = audio_get_file_data(&len);

    for (size_t pixel = 0; pixel < ((size_t) (gui.win_w - 200)); pixel++) {
        size_t start_index = (pixel + gui.start) * gui.zoom;

        // We reached the end of audio.
        if ((start_index + gui.zoom) * 2 >= len) {
            break;
        }

        float left_sample_pos = 0.0f;
        float left_sample_neg = 0.0f;
        float right_sample_pos = 0.0f;
        float right_sample_neg = 0.0f;
        for (size_t i = 0; i < gui.zoom; i++) {
            float left_sample = data[(i + start_index) * 2];
            float right_sample = data[(i + start_index) * 2 + 1];
            if (left_sample > 0.0f) {
                left_sample_pos += left_sample;
            } else {
                left_sample_neg += left_sample;
            }
            if (right_sample > 0.0f) {
                right_sample_pos += right_sample;
            } else {
                right_sample_neg += right_sample;
            }
        }

        left_sample_pos /= gui.zoom;
        left_sample_neg /= gui.zoom;
        right_sample_pos /= gui.zoom;
        right_sample_neg /= gui.zoom;
        float x = (((float) 100 + pixel) / gui.win_w) * 2.0 - 1.0;

        glColor3f(slice_color.x, slice_color.y, slice_color.z);
        glVertex2f(x, 0.5f);
        glVertex2f(x, 0.5 + left_sample_pos);
        glVertex2f(x, 0.5f);
        glVertex2f(x, 0.5 + left_sample_neg);

        glVertex2f(x, -0.5f);
        glVertex2f(x, -0.5 + right_sample_pos);
        glVertex2f(x, -0.5f);
        glVertex2f(x, -0.5 + right_sample_neg);
    }

    for (int i = 0; i < MAX_SLICES; i++) {
        Slice *slice = &gui.slices[i];
        switch (slice->state) {
            case SLICE_EMPTY:
                break;
            case SLICE_FIRST_MARK:
                draw_marker(slice->start, slice_color);
                break;
            case SLICE_FINISHED:
                draw_marker(slice->start, slice_color);
                draw_marker(slice->end, slice_color);
                break;
        }
    }

    draw_marker(gui.cursor_index, cursor_color);

    glEnd();

    SDL_GL_SwapWindow(gui.win);
}

static void draw_marker(size_t index, Vec3 color) {
    glColor3f(color.x, color.y, color.z);

    if (((float) index) / gui.zoom >= gui.start && (index - gui.start * gui.zoom) < (gui.win_w - 200) * gui.zoom) {
        float pixels_x = (((float) index) / gui.zoom) - gui.start + 100.0f;
        float index_x = (pixels_x / gui.win_w) * 2.0f - 1.0f;

        glVertex2f(index_x, 0.8f);
        glVertex2f(index_x, -0.8f);
    }
}

static int sdl_button_to_num(int button) {
    switch (button) {
        case SDL_BUTTON_LEFT:
            return BUTTON_LEFT;
        case SDL_BUTTON_RIGHT:
            return BUTTON_RIGHT;
        case SDL_BUTTON_MIDDLE:
            return BUTTON_MIDDLE;
        default:
            FAIL_FMT("unknown button: %d", button);
            return 0;
    }
}

static void gui_get_input() {
    gui.button_pressed[0] = gui.button_pressed[1] = gui.button_pressed[2] = false;
    gui.button_released[0] = gui.button_released[1] = gui.button_released[2] = false;

    for (int i = 0; i < _KEY_MAX; i++) {
        gui.key_pressed[i] = false;
        gui.key_released[i] = false;
    }

    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                gui.running = false;
                break;
            case SDL_MOUSEBUTTONDOWN: {
                int button_num = sdl_button_to_num(ev.button.button);
                gui.button_down[button_num] = true;
                gui.button_pressed[button_num] = true;
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                int button_num = sdl_button_to_num(ev.button.button);
                gui.button_down[button_num] = false;
                gui.button_released[button_num] = true;
                break;
            }
            case SDL_MOUSEMOTION:
                gui.mouse_x = ev.motion.x;
                gui.mouse_y = ev.motion.y;
                break;
            case SDL_KEYDOWN: {
                int key_num = sdl_key_to_num(ev.key.keysym.sym);
                if (key_num == -1) {
                    break;
                }
                if (gui.key_down[key_num]) {
                    break;
                }
                gui.key_down[key_num] = true;
                gui.key_pressed[key_num] = true;
                break;
            }
            case SDL_KEYUP: {
                int key_num = sdl_key_to_num(ev.key.keysym.sym);
                if (key_num == -1) {
                    break;
                }
                gui.key_down[key_num] = false;
                gui.key_released[key_num] = true;
                break;
            }
        }
    }
}

static int sdl_key_to_num(int key) {
    switch (key) {
        case SDLK_0:
            return KEY_0;
        case SDLK_1:
            return KEY_1;
        case SDLK_2:
            return KEY_2;
        case SDLK_3:
            return KEY_3;
        case SDLK_4:
            return KEY_4;
        case SDLK_5:
            return KEY_5;
        case SDLK_6:
            return KEY_6;
        case SDLK_7:
            return KEY_7;
        case SDLK_8:
            return KEY_8;
        case SDLK_9:
            return KEY_9;
        case SDLK_SPACE:
            return KEY_SPACE;
        case SDLK_LSHIFT:
            return KEY_SHIFT;
        case SDLK_ESCAPE:
            return KEY_ESC;
        default:
            return -1;
    }
}