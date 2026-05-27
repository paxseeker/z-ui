#ifndef Z_CLI_H
#define Z_CLI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Z_CLI_STRING,
    Z_CLI_INT,
    Z_CLI_FLOAT,
    Z_CLI_BOOL
} z_cli_type;

typedef struct {
    char short_name;
    const char *long_name;
    const char *description;
    z_cli_type type;
    bool is_flag;
    bool is_positional;
    int position;
    void *value;
    bool found;
} z_cli_arg;

typedef struct {
    const char *program_name;
    const char *version;
    const char *description;
    z_cli_arg *args;
    int arg_count;
    int arg_capacity;
    int positional_count;
    char **remaining_args;
    int remaining_count;
} z_cli_t;

void z_cli_init(z_cli_t *cli, const char *program_name, const char *version);
void z_cli_set_description(z_cli_t *cli, const char *description);
void z_cli_add_flag(z_cli_t *cli, char short_name, const char *long_name, const char *description, bool *value);
void z_cli_add_option(z_cli_t *cli, char short_name, const char *long_name, const char *description, z_cli_type type, void *value);
void z_cli_add_positional(z_cli_t *cli, const char *name, const char *description, z_cli_type type, void *value);
int z_cli_parse(z_cli_t *cli, int argc, char **argv);
void z_cli_print_help(z_cli_t *cli);
void z_cli_destroy(z_cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* Z_CLI_H */

#ifdef Z_CLI_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void z_cli_grow_args(z_cli_t *cli) {
    if (cli->arg_count >= cli->arg_capacity) {
        cli->arg_capacity = cli->arg_capacity == 0 ? 8 : cli->arg_capacity * 2;
        cli->args = (z_cli_arg*)realloc(cli->args, cli->arg_capacity * sizeof(z_cli_arg));
    }
}

void z_cli_init(z_cli_t *cli, const char *program_name, const char *version) {
    cli->program_name = program_name;
    cli->version = version;
    cli->description = NULL;
    cli->args = NULL;
    cli->arg_count = 0;
    cli->arg_capacity = 0;
    cli->positional_count = 0;
    cli->remaining_args = NULL;
    cli->remaining_count = 0;
}

void z_cli_set_description(z_cli_t *cli, const char *description) {
    cli->description = description;
}

void z_cli_add_flag(z_cli_t *cli, char short_name, const char *long_name, const char *description, bool *value) {
    z_cli_grow_args(cli);
    z_cli_arg *arg = &cli->args[cli->arg_count++];
    arg->short_name = short_name;
    arg->long_name = long_name;
    arg->description = description;
    arg->type = Z_CLI_BOOL;
    arg->is_flag = true;
    arg->is_positional = false;
    arg->position = -1;
    arg->value = value;
    *value = false;
    arg->found = false;
}

void z_cli_add_option(z_cli_t *cli, char short_name, const char *long_name, const char *description, z_cli_type type, void *value) {
    z_cli_grow_args(cli);
    z_cli_arg *arg = &cli->args[cli->arg_count++];
    arg->short_name = short_name;
    arg->long_name = long_name;
    arg->description = description;
    arg->type = type;
    arg->is_flag = false;
    arg->is_positional = false;
    arg->position = -1;
    arg->value = value;
    arg->found = false;
}

void z_cli_add_positional(z_cli_t *cli, const char *name, const char *description, z_cli_type type, void *value) {
    z_cli_grow_args(cli);
    z_cli_arg *arg = &cli->args[cli->arg_count++];
    arg->short_name = 0;
    arg->long_name = name;
    arg->description = description;
    arg->type = type;
    arg->is_flag = false;
    arg->is_positional = true;
    arg->position = cli->positional_count++;
    arg->value = value;
    arg->found = false;
}

static z_cli_arg *z_cli_find_short(z_cli_t *cli, char c) {
    for (int i = 0; i < cli->arg_count; i++) {
        if (cli->args[i].short_name == c) return &cli->args[i];
    }
    return NULL;
}

static z_cli_arg *z_cli_find_long(z_cli_t *cli, const char *s) {
    for (int i = 0; i < cli->arg_count; i++) {
        if (cli->args[i].long_name && strcmp(cli->args[i].long_name, s) == 0) return &cli->args[i];
    }
    return NULL;
}

static void z_cli_parse_value(z_cli_arg *arg, const char *s) {
    switch (arg->type) {
        case Z_CLI_STRING: *(const char**)arg->value = s; break;
        case Z_CLI_INT: *(int*)arg->value = (int)strtol(s, NULL, 10); break;
        case Z_CLI_FLOAT: *(float*)arg->value = (float)strtof(s, NULL); break;
        case Z_CLI_BOOL: *(bool*)arg->value = true; break;
    }
    arg->found = true;
}

int z_cli_parse(z_cli_t *cli, int argc, char **argv) {
    int i = 1;
    int positional_idx = 0;

    while (i < argc) {
        char *arg = argv[i];

        if (strcmp(arg, "--") == 0) {
            i++;
            break;
        }

        if (arg[0] == '-') {
            if (arg[1] == '-') {
                char *eq = strchr(arg + 2, '=');
                if (eq) {
                    *eq = '\0';
                    z_cli_arg *a = z_cli_find_long(cli, arg + 2);
                    if (!a) { fprintf(stderr, "Unknown option: %s\n", arg); return -1; }
                    z_cli_parse_value(a, eq + 1);
                    *eq = '=';
                } else {
                    z_cli_arg *a = z_cli_find_long(cli, arg + 2);
                    if (!a) { fprintf(stderr, "Unknown option: %s\n", arg); return -1; }
                    if (a->is_flag) {
                        *(bool*)a->value = true;
                        a->found = true;
                    } else if (i + 1 < argc) {
                        z_cli_parse_value(a, argv[++i]);
                    }
                }
            } else {
                for (int j = 1; arg[j]; j++) {
                    z_cli_arg *a = z_cli_find_short(cli, arg[j]);
                    if (!a) { fprintf(stderr, "Unknown option: -%c\n", arg[j]); return -1; }
                    if (a->is_flag) {
                        *(bool*)a->value = true;
                        a->found = true;
                    } else if (arg[j+1] == '=') {
                        z_cli_parse_value(a, arg + j + 2);
                        break;
                    } else if (arg[j+1]) {
                        z_cli_parse_value(a, arg + j + 1);
                        break;
                    } else if (i + 1 < argc) {
                        z_cli_parse_value(a, argv[++i]);
                        break;
                    }
                }
            }
        } else {
            for (int p = 0; p < cli->arg_count; p++) {
                if (cli->args[p].is_positional && !cli->args[p].found) {
                    z_cli_parse_value(&cli->args[p], arg);
                    break;
                }
            }
        }
        i++;
    }

    cli->remaining_count = argc - i;
    if (cli->remaining_count > 0) {
        cli->remaining_args = &argv[i];
    }

    return 0;
}

void z_cli_print_help(z_cli_t *cli) {
    printf("%s", cli->program_name);
    if (cli->version) printf(" %s", cli->version);
    printf("\n");
    if (cli->description) printf("\n%s\n", cli->description);
    printf("\nUsage: %s [options]", cli->program_name);

    for (int i = 0; i < cli->arg_count; i++) {
        if (cli->args[i].is_positional) {
            printf(" <%s>", cli->args[i].long_name);
        }
    }
    printf("\n\nOptions:\n");

    for (int i = 0; i < cli->arg_count; i++) {
        z_cli_arg *a = &cli->args[i];
        printf("  ");
        if (a->short_name) printf("-%c", a->short_name);
        if (a->short_name && a->long_name) printf(", ");
        if (a->long_name) {
            printf("--%s", a->long_name);
            if (!a->is_flag) printf(" <value>");
        }
        if (a->description) printf("\n        %s\n", a->description);
    }
}

void z_cli_destroy(z_cli_t *cli) {
    free(cli->args);
}

#endif /* Z_CLI_IMPLEMENTATION */
