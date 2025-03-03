#include "cinematic_brightness.h"

int main(int argc, char ** argv) {
    device = read_file("/opt/inkbox_device", true);
    // **** Arguments list ****
    //
    // 1                    2                    3                     4                    5
    // level_to_set (cold)  level_to_set (warm)  current_level (cold)  current_level(warm)  delay (microseconds)
    //
    // **** NOTE: if current_level(*) is set to -1, the program will attempt to retrieve the current brightness/warmth automatically. ****
    if(MATCH(device, "n873")) {
        cinematic_brightness(atoi(argv[1]), atoi(argv[3]), 0, atoi(argv[5]));
        cinematic_brightness(atoi(argv[2]), atoi(argv[4]), 1, atoi(argv[5]));
    }
    else {
        if(MATCH(device, "n249") || MATCH(device, "n418")) {
            cinematic_brightness(atoi(argv[2]), atoi(argv[4]), 1, atoi(argv[5]));
        }
        cinematic_brightness(atoi(argv[1]), atoi(argv[3]), 0, atoi(argv[5]));
    }
}

void cinematic_brightness(int level_to_set, int current_level, int type, int delay) {
    if(current_level < 0) {
        current_level = get_brightness(type);
    }
    char message_buff[256] = { 0 };
    snprintf(message_buff, sizeof(message_buff), "Set backlight (type %d) to level %d from current level %d", type, level_to_set, current_level);
    info(message_buff, INFO_OK);
    while(current_level != level_to_set) {
        if(current_level < level_to_set) {
            current_level++;
        }
        else {
            current_level--;
        }
        set_brightness(current_level, type);
        if(delay == -1) {
            delay = 5000;
        }
        usleep(delay);
    }
}
