#include "../common/functions.h"
char * device = NULL;
extern void set_brightness(int brightness, int mode);
extern int get_brightness(int mode);
void cinematic_brightness(int level_to_set, int current_level, int type, int delay);
