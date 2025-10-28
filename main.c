#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
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
int main(int argc, char** argv) {
    struct libevdev *dev = NULL;
    struct input_event ev;

    char* dev_path = calloc(1024, sizeof(char));
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

    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);

    // Initialize uinput device
    struct libevdev_uinput *uidev;
    int err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err < 0) {
        fprintf(stderr, "Failed to create uinput device: %s\n", strerror(-err));
        libevdev_free(dev);
        return -1;
    }

	// sleep 300ms
    usleep(300);

    ioctl(libevdev_get_fd(dev), EVIOCGRAB, 1);

    // define mouse status
    bool mouse = false;
    bool shift = false;

    int x = 0;
    int y = 0;

    do {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        printf("%d %d %d\n", ev.type, ev.code, ev.value);
        if (rc == 0 && ev.type == EV_KEY) {
            if (ev.code == KEY_RIGHTCTRL) {
                mouse = (ev.value > 0);
            }
            if (ev.code == KEY_LEFTSHIFT){
               shift = (ev.value > 0);
            }
            if(mouse){
                if (ev.code == KEY_W){ y = -1*ev.value; }
                if (ev.code == KEY_A){ x = -1*ev.value; }
                if (ev.code == KEY_S){ y = 1*ev.value; }
                if (ev.code == KEY_D){ x = 1*ev.value; }
                if (ev.code == KEY_Q){
                    libevdev_uinput_write_event(uidev, EV_KEY, BTN_LEFT, ev.value);
                    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }
                if (ev.code == KEY_E){
                    libevdev_uinput_write_event(uidev, EV_KEY, BTN_RIGHT, ev.value);
                    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }
                if (ev.code == KEY_LEFTCTRL || ev.code == KEY_LEFTALT || ev.code == KEY_LEFTSHIFT ||  ev.code == KEY_LEFTMETA){
                   libevdev_uinput_write_event(uidev, EV_KEY, ev.code, ev.value);
                   libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }
                if(ev.code == KEY_W || ev.code == KEY_A || ev.code == KEY_S || ev.code == KEY_D){
                    if(shift){
                        y = y*5;
                        x = x*5;
                    }
                    libevdev_uinput_write_event(uidev, EV_REL, REL_X, x*10);
                    libevdev_uinput_write_event(uidev, EV_REL, REL_Y, y*10);
                    libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
                }
            } else {
                libevdev_uinput_write_event(uidev, ev.type, ev.code, ev.value);
                libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
            }
        }
    } while (rc == 1 || rc == 0 || rc == -EAGAIN);

    libevdev_free(dev);
    close(fd);

    return 0;
}

