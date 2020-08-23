#include "libbip.h"
#include "unblockme.h"

struct regmenu_ menu_screen = { 55, 1, 0, dispatch_screen, keypress_screen, 0, 0, show_screen, 0, exit_game };
struct appdata_t** appdata_p;
struct appdata_t* appdata;
struct save_t saved_data;

int main(int p, char** a)
{
    show_screen((void*)p);
}

unsigned short randint(short max)
{
    appdata->randseed = appdata->randseed * get_tick_count();
    appdata->randseed++;
    return ((appdata->randseed >> 16) * max) >> 16;
}

void keypress_screen()
{
    if (appdata->state == STATE_SOLVED)
        return;

    appdata->state = appdata->state == STATE_PAUSE ? STATE_PLAY : STATE_PAUSE;
    draw_screen();
};

int get_bit_data(byte* board_data, int *byte_offset, int *bit_offset, int len)
{
    int value = 0;

    for (int i = 0; i < len; i++)
    {
        byte b = board_data[*byte_offset];
        value = (value << 1) | ((b & (1 << (7 - *bit_offset)) ? 1 : 0));

        (*bit_offset)++;
        if (*bit_offset == 8)
        {
            (*byte_offset)++;
            *bit_offset = 0;
        }
    }

    return value;
}

void set_board(int index)
{
    int bit_offset = 0;
    int byte_offset = 0;

    byte board_data[16 * 10 / 8 + 1];
    if (index < appdata->preloaded_start_index)
    {
        appdata->preloaded_start_index = index - PRELOADED_OFFSETS / 2;
        if (index - PRELOADED_OFFSETS / 2 < 0)
            appdata->preloaded_start_index = 0;
        read_elf_res_by_id(ELF_INDEX_SELF, RES_GAME_DATA, 2 * appdata->preloaded_start_index, appdata->preloaded_offsets, 2 * PRELOADED_OFFSETS);
    }
    else if (index > appdata->preloaded_start_index + PRELOADED_OFFSETS)
    {
        appdata->preloaded_start_index = index;
        read_elf_res_by_id(ELF_INDEX_SELF, RES_GAME_DATA, 2 * appdata->preloaded_start_index, appdata->preloaded_offsets, 2 * PRELOADED_OFFSETS);
    }

    read_elf_res_by_id(ELF_INDEX_SELF, RES_GAME_DATA, 2 * BOARD_COUNT + appdata->preloaded_offsets[index- appdata->preloaded_start_index], board_data, 16 * 10 / 8 + 1);

    appdata->board.item_count = get_bit_data(board_data, &byte_offset, &bit_offset, 4) + 2;
    for (int i = 0; i < appdata->board.item_count; i++)
    {
        appdata->board.items[i].red = !i ? 1 : 0;
        appdata->board.items[i].x = get_bit_data(board_data, &byte_offset, &bit_offset, 3);
        appdata->board.items[i].y = get_bit_data(board_data, &byte_offset, &bit_offset, 3);
        appdata->board.items[i].direction = get_bit_data(board_data, &byte_offset, &bit_offset, 1);
        appdata->board.items[i].length = get_bit_data(board_data, &byte_offset, &bit_offset, 2);
    }
}

int is_solved_board(int index)
{
    int byte_offset = index >> 3;
    int bit_offset = 7 - (index % 8);
    return (saved_data.board_soved[byte_offset] & (1 << bit_offset)) != 0;
}

void store_solved_index(int index)
{
    int byte_offset = index >> 3;
    int bit_offset = 7 - (index % 8);
    saved_data.board_soved[byte_offset] = saved_data.board_soved[byte_offset] | (1 << bit_offset);
}

void exit_game()
{
    saved_data.index = appdata->board_index;
    ElfWriteSettings(ELF_INDEX_SELF, &saved_data, 0, sizeof(struct save_t));

    //  если запуск был из быстрого меню, при нажатии кнопки выходим на циферблат
    if (get_left_side_menu_active())
        appdata->proc->ret_f = show_watchface;

    // вызываем функцию возврата (обычно это меню запуска), в качестве параметра указываем адрес функции нашего приложения
    show_menu_animate(appdata->ret_f, (unsigned int)show_screen, ANIMATE_RIGHT);
}

void init_game(int index_board)
{
    appdata->board_index = index_board;
    appdata->already_solved = is_solved_board(index_board);
    appdata->moves = 0;
    set_board(appdata->board_index);
    appdata->selected_item = 0;
    appdata->state = STATE_PLAY;
}

void show_screen(void* p)
{
    appdata_p = (struct appdata_t**)get_ptr_temp_buf_2();

    if ((p == *appdata_p) && get_var_menu_overlay()) {
        appdata = *appdata_p;
        *appdata_p = (struct appdata_t*)NULL;
        reg_menu(&menu_screen, 0);
        *appdata_p = appdata;
    }
    else {
        reg_menu(&menu_screen, 0);
        *appdata_p = (struct appdata_t*)pvPortMalloc(sizeof(struct appdata_t));
        appdata = *appdata_p;
        _memclr(appdata, sizeof(struct appdata_t));
        appdata->proc = (Elf_proc_*)p;
        appdata->randseed = get_tick_count();
    }

    if (p && appdata->proc->elf_finish)
        appdata->ret_f = appdata->proc->elf_finish;
    else
        appdata->ret_f = show_watchface;

    ElfReadSettings(ELF_INDEX_SELF, &saved_data, 0, sizeof(struct save_t));
    appdata->preloaded_start_index = saved_data.index;
    if (appdata->preloaded_start_index + PRELOADED_OFFSETS >= BOARD_COUNT)
        appdata->preloaded_start_index = BOARD_COUNT - PRELOADED_OFFSETS - 1;
    read_elf_res_by_id(ELF_INDEX_SELF, RES_GAME_DATA, 2 * appdata->preloaded_start_index, appdata->preloaded_offsets, 2 * PRELOADED_OFFSETS);

    init_game(saved_data.index);

    draw_screen();

    // не выключаем экран, не выключаем подсветку
    set_display_state_value(8, 1);
    set_display_state_value(4, 1);
    set_display_state_value(2, 0);
}

int get_item_index(int x_touch, int y_touch)
{
    for (int i = 0; i < appdata->board.item_count; i++)
    {
        int x = 11 + appdata->board.items[i].x * 26;
        int y = 18 + appdata->board.items[i].y * 26;
        int width, height;
        int value = appdata->board.items[i].length * 24 + (2 * (appdata->board.items[i].length - 1));
        if (appdata->board.items[i].direction)
        {
            width = value + 1;
            height = 24 + 1;
        }
        else
        {
            width = 24 + 1;
            height = value + 1;
        }

        if ((x_touch >= x) && (x_touch <= x + width) && (y_touch >= y) && (y_touch <= y + height))
            return i;
    }
    return -1;
}

void move_item(int index, int direction)
{
    if (index == -1)
        return;

    int xx, yy;
    if ((direction == GESTURE_SWIPE_UP || direction == GESTURE_SWIPE_DOWN) && appdata->board.items[index].direction == ITEM_HORIZONTAL)
        return;
    if ((direction == GESTURE_SWIPE_LEFT || direction == GESTURE_SWIPE_RIGHT) && appdata->board.items[index].direction == ITEM_VERTICAL)
        return;

    byte board[6][6];
    _memclr(board, sizeof(board));

    for (int i = 0; i < appdata->board.item_count; i++)
    {
        for (int j = 0; j < appdata->board.items[i].length; j++)
        {
            if (appdata->board.items[i].direction == ITEM_HORIZONTAL)
            {
                board[appdata->board.items[i].x+j][appdata->board.items[i].y] = 1;
            }
            else 
            {
                board[appdata->board.items[i].x][appdata->board.items[i].y+j] = 1;
            }
        }
    }

    switch (direction)
    {
    case GESTURE_SWIPE_UP:
        if (appdata->board.items[index].y > 0 && !board[appdata->board.items[index].x][appdata->board.items[index].y - 1])
        {
            appdata->board.items[index].y--;
            appdata->moves++;
        }
        break;
    case GESTURE_SWIPE_DOWN:
        yy = appdata->board.items[index].y + appdata->board.items[index].length;
        if (yy >= 6)
            break;
        if (appdata->board.items[index].y < 5 && !(board[appdata->board.items[index].x][yy]))
        {
            appdata->board.items[index].y++;
            appdata->moves++;
        }
        break;
    case GESTURE_SWIPE_RIGHT:
        if (index == 0 && appdata->board.items[0].x == 4 && appdata->board.items[0].y == 2)
        {
            appdata->state = STATE_SOLVED;
            store_solved_index(appdata->board_index);
            appdata->already_solved = 1;
            break;
        }

        xx = appdata->board.items[index].x + appdata->board.items[index].length;
        if (xx >= 6)
            break;
        if (appdata->board.items[index].x < 5 && !(board[xx][appdata->board.items[index].y]))
        {
            appdata->board.items[index].x++;
            appdata->moves++;
        }
        break;
    case GESTURE_SWIPE_LEFT:
        if (appdata->board.items[index].x > 0 && !board[appdata->board.items[index].x - 1][appdata->board.items[index].y])
        {
            appdata->board.items[index].x--;
            appdata->moves++;
        }
        break;
    }
}

int dispatch_screen(void* p)
{
    struct gesture_* gest = (struct gesture_*)p;

    switch (appdata->state)
    {
        case STATE_PLAY:
        {
            switch (gest->gesture)
            {
            case GESTURE_CLICK:
                appdata->selected_item = get_item_index(gest->touch_pos_x, gest->touch_pos_y);
                break;
            case GESTURE_SWIPE_UP:
                move_item(appdata->selected_item, GESTURE_SWIPE_UP);
                break;
            case GESTURE_SWIPE_DOWN:
                move_item(appdata->selected_item, GESTURE_SWIPE_DOWN);
                break;
            case GESTURE_SWIPE_LEFT:
                move_item(appdata->selected_item, GESTURE_SWIPE_LEFT);
                break;
            case GESTURE_SWIPE_RIGHT:
                move_item(appdata->selected_item, GESTURE_SWIPE_RIGHT);
                break;
            default:
                break;
            }
            break;
        }

        case STATE_PAUSE:
        {
            if (gest->gesture != GESTURE_CLICK)
                break;

            if (gest->touch_pos_x > 44 && gest->touch_pos_x < 88 && gest->touch_pos_y > 44 && gest->touch_pos_y < 88)
            {
                if (appdata->board_index > 0)
                {
                    init_game(appdata->board_index - 1);
                }
                else
                    init_game(0);
            }
            else if (gest->touch_pos_x > 88 && gest->touch_pos_x < 132 && gest->touch_pos_y > 44 && gest->touch_pos_y < 88)
            {
                if (appdata->board_index < BOARD_COUNT - 1)
                {
                    init_game(appdata->board_index + 1);
                }
                else
                    init_game(BOARD_COUNT - 1);
            }
            else if (gest->touch_pos_x > 44 && gest->touch_pos_x < 88 && gest->touch_pos_y > 88 && gest->touch_pos_y < 132)
            {
                exit_game();
                return 0;
            }
            else if (gest->touch_pos_x > 88 && gest->touch_pos_x < 132 && gest->touch_pos_y > 88 && gest->touch_pos_y < 132)
            {
                init_game(appdata->board_index);
            }

            break;
        }

        case STATE_SOLVED:

            if (gest->gesture != GESTURE_CLICK)
                break;

            if (gest->touch_pos_x > 125 && gest->touch_pos_x < 144 && gest->touch_pos_y > 93 && gest->touch_pos_y < 112)
            {
                if (appdata->board_index < BOARD_COUNT - 1)
                {
                    init_game(appdata->board_index + 1);
                }
                else
                    init_game(BOARD_COUNT - 1);
            }
            else if (gest->touch_pos_x > 39 && gest->touch_pos_x < 58 && gest->touch_pos_y > 93 && gest->touch_pos_y < 112)
            {
                exit_game();
                return 0;
            }
            else if (gest->touch_pos_x > 82 && gest->touch_pos_x < 101 && gest->touch_pos_y > 93 && gest->touch_pos_y < 112)
            {
                init_game(appdata->board_index);
            }
            break;
    }

    draw_screen();
    return 0;
}

int get_digits_width(byte value, int spacing)
{
    struct res_params_ res_params;
    int width = 0;
    int max = 100000;
    while (max)
    {
        if (value / max)
            break;
        max = max / 10;
    }

    do
    {
        int mm = max == 0 ? 0 : value / max;

        get_res_params(ELF_INDEX_SELF, mm, &res_params);
        width += res_params.width + spacing;
        if (max == 0)
            break;

        value = value % max;
        max = max / 10;
    } while (max);

    return width - spacing;
}

int print_digits(int value, int x, int y, int spacing)
{
    struct res_params_ res_params;

    int max = 100000;
    int x_store = x;
    while (max)
    {
        if (value / max)
            break;
        max = max / 10;
    }

    do
    {
        int mm = max == 0 ? 0 : value / max;

        get_res_params(ELF_INDEX_SELF, mm, &res_params);
        show_elf_res_by_id(ELF_INDEX_SELF, mm, x, y);
        x += res_params.width + spacing;
        if (max == 0)
            break;

        value = value % max;
        max = max / 10;
    } while (max);

    return x - x_store - spacing;
}


void draw_screen()
{
    set_graph_callback_to_ram_1();
    set_bg_color(COLOR_BLACK);
    fill_screen_bg();
    set_fg_color(COLOR_WHITE);
    draw_filled_rect(0, 14, 176, 176);

    set_fg_color(COLOR_BLACK);
    draw_filled_rect(9, 16, 167, 174);
    draw_filled_rect(167, 68, 176, 96);
    draw_filled_rect(167, 67, 168, 68);
    draw_filled_rect(167, 96, 168, 97);

    set_fg_color(COLOR_WHITE);
    draw_filled_rect(9, 16, 10, 17);
    draw_filled_rect(166, 16, 167, 17);
    draw_filled_rect(9, 173, 10, 174);
    draw_filled_rect(166, 173, 167, 174);

    for (int i = 0; i < appdata->board.item_count; i++)
    {
        int x = 11 + appdata->board.items[i].x * 26;
        int y = 18 + appdata->board.items[i].y * 26;
        int width, height;
        int value = appdata->board.items[i].length * 24 + (2 * (appdata->board.items[i].length - 1));
        if (appdata->board.items[i].direction)
        {
            width = value;
            height = 24;
        }
        else 
        {
            width = 24;
            height = value;
        }

        set_fg_color(appdata->board.items[i].red ? COLOR_RED : COLOR_YELLOW);
        draw_filled_rect(x, y, x + width, y + height);
        if (i == appdata->selected_item && appdata->state != STATE_SOLVED)
        {
            set_fg_color(appdata->board.items[i].red ? COLOR_WHITE : COLOR_GREEN);
            draw_rect(x, y, x + width-1, y + height-1);
            draw_rect(x+1, y+1, x + width - 2, y + height - 2);
        }

        set_fg_color(COLOR_BLACK);
        draw_filled_rect(x, y, x + 1, y + 1);
        draw_filled_rect(x + width - 1, y, x + width, y + 1);
        draw_filled_rect(x, y + height - 1, x + 1, y + height + 1);
        draw_filled_rect(x + width - 1, y + height - 1, x + width, y + height + 1);
    }

    int w = print_digits(appdata->board_index + 1, 14, 2, 2);
    show_elf_res_by_id(ELF_INDEX_SELF, RES_DELIM, 16+w, 2);
    print_digits(BOARD_COUNT, 25+w, 2, 2);

    set_fg_color(appdata->already_solved ? COLOR_GREEN :  COLOR_RED);
    draw_filled_rect(5, 4, 11, 10);

    w = get_digits_width(appdata->moves, 2);
    print_digits(appdata->moves, 170 - w, 2, 2);

    if (appdata->state == STATE_PAUSE)
    {
        set_bg_color(COLOR_WHITE);
        draw_filled_rect_bg(44,44,132,132);
        set_bg_color(COLOR_BLUE);
        draw_filled_rect_bg(46, 46, 131, 131);

        show_elf_res_by_id(ELF_INDEX_SELF, RES_PREVIOUS, 58, 58);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_NEXT, 97, 58);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_TURN_OFF, 58, 97);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_RESET, 99, 99);
    }
    else if (appdata->state == STATE_SOLVED)
    {
        set_bg_color(COLOR_WHITE);
        draw_filled_rect_bg(26, 56, 156, 121);
        set_bg_color(COLOR_GREEN);
        draw_filled_rect_bg(28, 58, 155, 120);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_SOLVED, 43, 65);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_TURN_OFF, 39, 93);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_RESET, 82, 93);
        show_elf_res_by_id(ELF_INDEX_SELF, RES_NEXT, 125, 93);

        set_fg_color(COLOR_RED);
        draw_filled_rect(164, 70, 176, 94);
    }

    repaint_screen_lines(1, 176);
}
