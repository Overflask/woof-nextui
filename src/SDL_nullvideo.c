#include "SDL_internal.h"
#ifdef SDL_VIDEO_DRIVER_DUMMY
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../joystick/SDL_sysjoystick.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include "SDL_nullvideo.h"
#include "SDL_nullevents_c.h"
#include "SDL_nullframebuffer_c.h"

static int *input_fds = NULL;
static int num_input_fds = 0;
static bool is_trimui = false;
static SDL_JoystickID virt_joy_id = 0;
static SDL_Joystick *virt_joy = NULL;
static volatile sig_atomic_t save_and_quit_stage = 0;
static Uint64 save_quit_timer = 0;

static void handle_sigterm(int sig) {
    (void)sig;
    if (save_and_quit_stage == 0) {
        FILE *pf = fopen("/tmp/woof_poweroff", "w");
        if (pf) fclose(pf);
        save_and_quit_stage = 1;
    }
}

static void init_virtual_gamepad(void) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = 16;
    desc.name = "NextUI Handheld Pad";
    virt_joy_id = SDL_AttachVirtualJoystick(&desc);
    if (virt_joy_id != 0) {
        virt_joy = SDL_OpenJoystick(virt_joy_id);
    }
}

static void open_nextui_inputs(void) {
    DIR *dir = opendir("/dev/input");
    struct dirent *ent;
    if (!input_fds) input_fds = (int *)SDL_calloc(32, sizeof(int));
    num_input_fds = 0;
    if (!dir || !input_fds) return;

    while ((ent = readdir(dir)) != NULL && num_input_fds < 32) {
        if (SDL_strncmp(ent->d_name, "event", 5) == 0) {
            char *path = (char *)SDL_malloc(128);
            if (path) {
                SDL_snprintf(path, 128, "/dev/input/%s", ent->d_name);
                int fd = open(path, O_RDONLY | O_NONBLOCK);
                if (fd >= 0) {
                    char *name = (char *)SDL_calloc(1, 128);
                    bool skip = false;
                    if (name) {
                        ioctl(fd, EVIOCGNAME(127), name);
                        if (SDL_strstr(name, "Trimui") != NULL || SDL_strstr(name, "TRIMUI") != NULL || SDL_strstr(name, "trimui") != NULL) {
                            is_trimui = true;
                        }
                        if (SDL_strstr(name, "dierct") != NULL || SDL_strstr(name, "direct") != NULL) {
                            skip = true;
                        }
                        SDL_free(name);
                    }
                    if (skip) close(fd);
                    else { *(input_fds + num_input_fds) = fd; num_input_fds++; }
                }
                SDL_free(path);
            }
        }
    }
    closedir(dir);
    signal(SIGTERM, is_trimui ? handle_sigterm : SIG_IGN);
    signal(SIGINT, handle_sigterm);
    init_virtual_gamepad();
}

static void close_nextui_inputs(void) {
    if (virt_joy) { SDL_CloseJoystick(virt_joy); virt_joy = NULL; }
    if (virt_joy_id != 0) { SDL_DetachVirtualJoystick(virt_joy_id); virt_joy_id = 0; }
    if (!input_fds) return;
    for (int i = 0; i < num_input_fds; i++) {
        int fd = *(input_fds + i);
        if (fd >= 0) { close(fd); *(input_fds + i) = -1; }
    }
    num_input_fds = 0;
}

static void send_gamepad_btn(uint16_t code, bool p) {
    if (!virt_joy) return;
    if (is_trimui) {
        switch (code) {
            case 304: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_SOUTH, p); break;          /* Physical A <-> B swapped */
            case 305: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_EAST, p); break;           /* Physical B <-> A swapped */
            case 307: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_WEST, p); break;           /* Physical X <-> Y swapped */
            case 308: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_NORTH, p); break;          /* Physical Y <-> X swapped */
            case 310: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, p); break;  /* L1 */
            case 311: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, p); break; /* R1 */
            case 314: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_BACK, p); break;           /* Select */
            case 315: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_START, p); break;          /* Start */
            case 312: case 316: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_LEFT_STICK, p); break;  /* L2 -> Prev Weapon */
            case 313: case 317: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_RIGHT_STICK, p); break; /* R2 -> Next Weapon */
            case KEY_UP: case BTN_DPAD_UP: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_UP, p); break;
            case KEY_DOWN: case BTN_DPAD_DOWN: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_DOWN, p); break;
            case KEY_LEFT: case BTN_DPAD_LEFT: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, p); break;
            case KEY_RIGHT: case BTN_DPAD_RIGHT: SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, p); break;
            default: break;
        }
        return;
    }
    switch (code) {
        case 304: /* Physical A -> SOUTH (pada): Menu Confirm + Use + Strafe */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_SOUTH, p); break;
        case 305: /* Physical B -> EAST (padb): Menu Back + Fire */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_EAST, p); break;
        case 307: /* Physical X -> WEST (padx): Automap Follow */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_WEST, p); break;
        case 306: /* Physical Y -> NORTH (pady): Run / Speed */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_NORTH, p); break;
        case 310: /* Select -> BACK (back): Toggle Automap */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_BACK, p); break;
        case 311: /* Start -> START (start): Menu (312 Menu button disabled) */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_START, p); break;
        case 308: /* L1 -> LEFT_SHOULDER (lb): Strafe Left */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, p); break;
        case 309: /* R1 -> RIGHT_SHOULDER (rb): Strafe Right */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, p); break;
        case 314: /* L2 -> ls (button 7): Prev Weapon */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_LEFT_STICK, p); break;
        case 315: /* R2 -> rs (button 8): Next Weapon */
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_RIGHT_STICK, p); break;
        case KEY_UP: case BTN_DPAD_UP:
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_UP, p); break;
        case KEY_DOWN: case BTN_DPAD_DOWN:
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_DOWN, p); break;
        case KEY_LEFT: case BTN_DPAD_LEFT:
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, p); break;
        case KEY_RIGHT: case BTN_DPAD_RIGHT:
            SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, p); break;
        default: break;
    }
}

static void nextui_pump_events(SDL_VideoDevice *_this) {
    struct input_event ev;
    static bool hu = false, hd = false, hl = false, hr = false;
    (void)_this;

    /* Quicksave + Quit only when NextUI sends SIGTERM during system shutdown */
    if (save_and_quit_stage > 0) {
        Uint64 now = SDL_GetTicks();
        if (save_and_quit_stage == 1) {
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_F6, true);
            save_quit_timer = now; save_and_quit_stage = 2;
        } else if (save_and_quit_stage == 2 && now - save_quit_timer >= 15) {
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_F6, false);
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_RETURN, true);
            save_quit_timer = now; save_and_quit_stage = 3;
        } else if (save_and_quit_stage == 3 && now - save_quit_timer >= 15) {
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_RETURN, false);
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_RETURN, true);
            save_quit_timer = now; save_and_quit_stage = 4;
        } else if (save_and_quit_stage == 4 && now - save_quit_timer >= 15) {
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_RETURN, false);
            save_quit_timer = now; save_and_quit_stage = 5;
        } else if (save_and_quit_stage == 5 && now - save_quit_timer >= 40) {
            SDL_SendQuit(); save_and_quit_stage = 6;
        }
    }

    if (!input_fds) return;
    for (int i = 0; i < num_input_fds; i++) {
        int fd = *(input_fds + i);
        if (fd < 0) continue;
        while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY) {
                if ((ev.code == 354 || ev.code == KEY_POWER) && ev.value == 1) {
                    if (save_and_quit_stage == 0) {
                        FILE *pf = fopen("/tmp/woof_poweroff", "w");
                        if (pf) fclose(pf);
                        save_and_quit_stage = 1;
                    }
                    continue;
                }
                if (ev.value == 1) send_gamepad_btn(ev.code, true);
                else if (ev.value == 0) send_gamepad_btn(ev.code, false);
            } else if (ev.type == EV_ABS && virt_joy) {
                if (ev.code == ABS_HAT0X) {
                    if ((ev.value < 0) != hl) { hl = (ev.value < 0); SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_LEFT, hl); }
                    if ((ev.value > 0) != hr) { hr = (ev.value > 0); SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, hr); }
                } else if (ev.code == ABS_HAT0Y) {
                    if ((ev.value < 0) != hu) { hu = (ev.value < 0); SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_UP, hu); }
                    if ((ev.value > 0) != hd) { hd = (ev.value > 0); SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_DPAD_DOWN, hd); }
                } else if (ev.code == ABS_X) {
                    Sint16 val = (Sint16)(ev.value < -32768 ? -32768 : (ev.value > 32767 ? 32767 : ev.value));
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_LEFTX, val);
                } else if (ev.code == ABS_Y) {
                    Sint16 val = (Sint16)(ev.value < -32768 ? -32768 : (ev.value > 32767 ? 32767 : ev.value));
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_LEFTY, val);
                } else if (ev.code == ABS_RX) {
                    Sint16 val = (Sint16)(ev.value < -32768 ? -32768 : (ev.value > 32767 ? 32767 : ev.value));
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_RIGHTX, val);
                } else if (ev.code == ABS_RY) {
                    Sint16 val = (Sint16)(ev.value < -32768 ? -32768 : (ev.value > 32767 ? 32767 : ev.value));
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_RIGHTY, val);
                } else if ((ev.code == ABS_Z || ev.code == 2) && is_trimui) {
                    Sint16 tval = (ev.value > 128) ? 32767 : 0;
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, tval);
                    SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_LEFT_STICK, ev.value > 128);
                } else if ((ev.code == ABS_RZ || ev.code == 5) && is_trimui) {
                    Sint16 tval = (ev.value > 128) ? 32767 : 0;
                    SDL_SetJoystickVirtualAxis(virt_joy, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, tval);
                    SDL_SetJoystickVirtualButton(virt_joy, SDL_GAMEPAD_BUTTON_RIGHT_STICK, ev.value > 128);
                }
            }
        }
    }
}

static bool DUMMY_SetWindowPosition(SDL_VideoDevice *_this, SDL_Window *w) {
    (void)_this; SDL_SendWindowEvent(w, SDL_EVENT_WINDOW_MOVED, w->pending.x, w->pending.y); return true;
}
static void DUMMY_SetWindowSize(SDL_VideoDevice *_this, SDL_Window *w) {
    (void)_this; SDL_SendWindowEvent(w, SDL_EVENT_WINDOW_RESIZED, w->pending.w, w->pending.h);
}
static void DUMMY_DeleteDevice(SDL_VideoDevice *d) { SDL_free(d); }
static bool DUMMY_SetRelativeMouseMode(bool e) { (void)e; return true; }

static bool DUMMY_VideoInit(SDL_VideoDevice *_this) {
    SDL_DisplayMode mode;
    int w = 640, h = 480;
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == 0 && vinfo.xres > 0 && vinfo.yres > 0) {
            if (vinfo.xres < vinfo.yres) { w = (int)vinfo.yres; h = (int)vinfo.xres; }
            else { w = (int)vinfo.xres; h = (int)vinfo.yres; }
        }
        close(fbfd);
    }
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.w = w; mode.h = h;
    if (SDL_AddBasicVideoDisplay(&mode) == 0) return false;
    SDL_GetMouse()->SetRelativeMouseMode = DUMMY_SetRelativeMouseMode;
    open_nextui_inputs();
    return true;
}

static void DUMMY_VideoQuit(SDL_VideoDevice *_this) { (void)_this; close_nextui_inputs(); }

static SDL_VideoDevice *DUMMY_InternalCreateDevice(const char *name) {
    const char *hint = SDL_GetHint(SDL_HINT_VIDEO_DRIVER);
    if (!hint || SDL_strstr(hint, name) == NULL) return NULL;
    SDL_VideoDevice *d = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!d) return NULL;
    d->VideoInit = DUMMY_VideoInit;
    d->VideoQuit = DUMMY_VideoQuit;
    d->PumpEvents = nextui_pump_events;
    d->SetWindowSize = DUMMY_SetWindowSize;
    d->SetWindowPosition = DUMMY_SetWindowPosition;
    d->CreateWindowFramebuffer = SDL_DUMMY_CreateWindowFramebuffer;
    d->UpdateWindowFramebuffer = SDL_DUMMY_UpdateWindowFramebuffer;
    d->DestroyWindowFramebuffer = SDL_DUMMY_DestroyWindowFramebuffer;
    d->free = DUMMY_DeleteDevice;
    return d;
}

static SDL_VideoDevice *DUMMY_CreateDevice(void) { return DUMMY_InternalCreateDevice("dummy"); }
VideoBootStrap DUMMY_bootstrap = { "dummy", "SDL NextUI fbdev driver", DUMMY_CreateDevice, NULL, false };
#ifdef SDL_INPUT_LINUXEV
static SDL_VideoDevice *DUMMY_EVDEV_CreateDevice(void) { return DUMMY_InternalCreateDevice("evdev"); }
VideoBootStrap DUMMY_evdev_bootstrap = { "evdev", "SDL NextUI fbdev+evdev driver", DUMMY_EVDEV_CreateDevice, NULL, false };
#endif
#endif
