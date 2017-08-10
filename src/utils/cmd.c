#include "cmd.h"

static bool quit = false;

char *all[10] = {"cat", "ls", "lscpu", "lspci", "lsusb"};

char *
dupstr(char *s)
{
    char *r = calloc(strlen(s) + 1, 1);
    strcpy(r, s);
    return r;
}

char *
command_generator(const char *text, int32_t state)
{
    static int32_t list_index, len;
    char *name;
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    while ((name = all[list_index])) {
        list_index ++;
        if (strncmp(name, text, len) == 0) {
            return dupstr(name);
        }
    }
    return NULL;
}

char **
completion(const char *text, int32_t start, int32_t end)
{
    char **matches = NULL;

    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    }
    return matches;
}

void
zen_init_readline()
{
    rl_readline_name = "swift_ctl";
    rl_attempted_completion_function = completion;
}

char *
zen_strip_white(char *string)
{
    register char *s, *t;
    for (s = string; whitespace(*s); s++) {
        ;
    }
    if (*s == 0) {
        return s;
    }
    t = s + strlen(s) - 1;
    while (t > s && whitespace(*t)) {
        t--;
    }
    *++t = '\0';
    return s;
}

void
zen_del_old_cmd(const char *line)
{
    HIST_ENTRY **list = history_list();
    if (!list) {
        return;
    }
    for (uint8_t i = 0; list[i]; i++) {
        if (!strcmp(line, list[i]->line)) {
            remove_history(i);
        }
    }
}

