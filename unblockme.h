#ifndef __UNBLOCK_ME__
#define __UNBLOCK_ME__

#include "libbip.h"

typedef unsigned short word;
typedef unsigned char byte;

struct item_t
{
    byte red;
    byte x;
    byte y;
    byte direction;
    byte length;
};

struct board_t
{
    byte item_count;
    struct item_t items[16];
};

#define PRELOADED_OFFSETS 50

struct appdata_t
{
    Elf_proc_* proc;
    void* ret_f;
    word board_index;
    struct board_t board;
    word preloaded_offsets[PRELOADED_OFFSETS];
    int preloaded_start_index;
    int selected_item;
    int already_solved;
    word moves;
    int randseed;
    int state;
};

struct save_t
{
    word index;
    byte board_soved[250];
};

// RES's

#define RES_DELIM 10
#define RES_NEXT 11
#define RES_PREVIOUS 12
#define RES_RESET 13
#define RES_SOLVED 14
#define RES_TURN_OFF 15
#define RES_GAME_DATA 16


#define STATE_PLAY 0
#define STATE_SOLVED 1
#define STATE_PAUSE 2

#define BOARD_COUNT 2000

#define ITEM_HORIZONTAL 1
#define ITEM_VERTICAL 0

void show_screen(void* return_screen);
void keypress_screen();
int dispatch_screen(void* p);
void draw_screen();
void exit_game();

#endif
