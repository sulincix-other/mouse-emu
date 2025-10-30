#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

void list_devices() {
    DIR *dir;
    struct dirent *ent;
    char path[1024];
    printf("Searching:\n");
    if ((dir = opendir("/dev/input/")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strncmp(ent->d_name, "event", 5) == 0) {
                sprintf(path, "/dev/input/%s", ent->d_name);
                int fd = open(path, O_RDONLY);
                if(fd < 0){
                    continue;
                }
                struct libevdev *dev = NULL;
                if (libevdev_new_from_fd(fd, &dev) < 0) {
                    close(fd);
                    continue;
                }
                if (libevdev_has_event_type(dev, EV_KEY) &&
                    libevdev_has_event_code(dev, EV_KEY, KEY_A)) {
                    printf("%s => %s\n", path, libevdev_get_name(dev));
                }
                libevdev_free(dev);
                close(fd);
            }
        }
        closedir(dir);
    }
}
typedef struct {
    int x;
    int y;
    int type;
    int code;
    int value;
    bool shift;
    bool mouse;
} Event;

static Event ev;
pthread_mutex_t lock;
static struct libevdev_uinput *uidev;

static void do_event_fn(int type, int code, int value){
    pthread_mutex_lock(&lock);
    libevdev_uinput_write_event(uidev, type, code, value);
    pthread_mutex_unlock(&lock);
}

static void do_event(int type, int code, int value){
    //printf("%d %d %d\n", type, code, value);
    do_event_fn(type, code, value);
    do_event_fn(EV_SYN, SYN_REPORT, 0);
    usleep(50);
}

static bool loop_enabled = false;
static void* loop(void* arg) {
    pthread_mutex_lock(&lock);
    if(loop_enabled){
        return NULL;
    }
    loop_enabled = true;
    pthread_mutex_unlock(&lock);
    (void) arg;
    while(1){
        int slow = 1;
        if(ev.shift){
            slow = 5;
        }
        usleep(1331*slow);
        if(!ev.mouse){
            pthread_mutex_lock(&lock);
            loop_enabled = false;
            pthread_mutex_unlock(&lock);
            break;
        }
        if(ev.x != 0 || ev.y != 0){
            if(ev.x != 0){
                do_event_fn( EV_REL, REL_X, ev.x);
            }
            if(ev.y != 0){
                do_event_fn(EV_REL, REL_Y, ev.y);
            }
            do_event_fn(EV_SYN, SYN_REPORT, 0);
        }
        if(ev.code != 0){
            do_event(ev.type, ev.code, ev.value);
            ev.code = 0;
        }
    }
    return NULL;
}



static void process_event(struct input_event e){
    // X axis
    if (e.code == KEY_A){
        ev.x = -1 * e.value;
    } else if(e.code == KEY_D){
        ev.x = e.value;
    } else if (e.code == KEY_W){
        ev.y = -1 * e.value;
    } else if(e.code == KEY_S){
        ev.y = e.value;
    }
    // Clicks
    ev.code = 0;
    if(e.value == 1 || e.value == 0){
        ev.value = e.value;
        ev.type = EV_KEY;
        if(e.code == KEY_Q){
            ev.code = BTN_LEFT;
        }else if(e.code == KEY_E){
            ev.code = BTN_RIGHT;
        }else if(e.code == KEY_R){
            ev.code = BTN_MIDDLE;
        }else if(e.code == KEY_HOME){
            ev.code = BTN_EXTRA;
        }else if(e.code == KEY_END){
            ev.code = BTN_SIDE;
        }
    }
    if(e.code == KEY_PAGEDOWN){
        ev.type = EV_REL;
        ev.code = REL_WHEEL_HI_RES;
        ev.value = -120*(e.value > 0);
    }else if(e.code == KEY_PAGEUP){
        ev.type = EV_REL;
        ev.code = REL_WHEEL_HI_RES;
        ev.value = 120*(e.value > 0);
    }
}


static int buttons_status[512];

int main(int argc, char** argv) {
    struct libevdev *dev = NULL;
    struct input_event e;
    // reset all status
    for(size_t i=0; i<512;i++){
        buttons_status[i] = 0;
    }

    char dev_path[PATH_MAX];
    if (argc < 2) {
        list_devices();
        return 0;
    } else {
        strncpy(dev_path, argv[1], 1024);
    }

    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        exit(1);
    }

    int rc = libevdev_new_from_fd(fd, &dev);
    if (rc < 0) {
        perror("Failed to initialize libevdev");
        close(fd);
        exit(1);
    }

    // replace vendor info
    libevdev_set_name(dev, "Mouse Emulator");
    libevdev_set_id_vendor(dev, 0x1453);
    libevdev_set_id_product(dev, 0x1299);

    libevdev_enable_event_type(dev, EV_REL);
    libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_WHEEL_HI_RES, NULL);

    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_EXTRA, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_SIDE, NULL);

    // Initialize uinput device
    int err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-err));
        libevdev_free(dev);
        return -1;
    }

	// sleep 300ms
    usleep(300);

    ioctl(libevdev_get_fd(dev), EVIOCGRAB, 1);


    do {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &e);
        //printf("%d %d %d\n", ev.type, ev.code, ev.value);
        if (rc == 0 && e.type == EV_KEY) {
            buttons_status[e.code] = e.value;
            if (e.code == KEY_RIGHTCTRL) {
                for(size_t i=0; i<512;i++){
                    if(buttons_status[i]){
                        do_event(EV_KEY, i, 0);
                        buttons_status[e.code] = 0;
                    }
                }
                bool m = ev.mouse;
                ev.mouse = (e.value > 0);
                if(ev.mouse && !m){
                    pthread_t thread;
                    pthread_create(&thread, NULL, loop, 0);
                }
            }
            if (e.code == KEY_LEFTSHIFT || e.code == KEY_RIGHTSHIFT){
               ev.shift = (e.value > 0);
            }
            if(ev.mouse){
                process_event(e);
                if (e.code == KEY_LEFTCTRL || e.code == KEY_LEFTALT || e.code == KEY_LEFTSHIFT ||  e.code == KEY_LEFTMETA){
                   do_event(EV_KEY, e.code, e.value);
                }
            } else {
                   do_event(EV_KEY, e.code, e.value);
            }
        }
    } while (rc == 1 || rc == 0 || rc == -EAGAIN);

    pthread_mutex_destroy(&lock);
    libevdev_free(dev);
    close(fd);

    return 0;
}

