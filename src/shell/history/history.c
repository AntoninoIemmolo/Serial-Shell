
#include "history.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INITIAL_DIM 256
#define MAX_STR_LEN 256

struct history {
    // the array of string, the actual history
    char** history;
    // the number of elemets that the history can store
    size_t maxDim;
    // the number of elemets in the history
    size_t len;
    // the index in the history array
    size_t idx;
};

static int8_t historyRealloc(history_t h);

void historyPush(history_t h, char* str)
{
    if (h == NULL) {
        fprintf(stderr, "ERROR: history push error history can not be a pointer to NULL\r\n");
        fflush(stderr);
    } else if (str == NULL) {
        fprintf(stderr, "ERROR: history push error str can not be a pointer to NULL\r\n");
        fflush(stderr);
    } else if (strlen(str) > MAX_STR_LEN) {
        fprintf(stderr, "ERROR: history push error string of len %zu is to big to be stored in history\r\n", strlen(str));
        fflush(stderr);
    } else {
        if (h->len >= h->maxDim) {
            historyRealloc(h);
        }
        h->history[h->len] = malloc(sizeof(*(h->history)) * MAX_STR_LEN);
        memccpy(h->history[h->len], str, strlen(str), MAX_STR_LEN);
        h->len++;
        h->idx++;
    }
    return;
}

void historyReset(history_t h)
{
    if (h == NULL) {
        fprintf(stderr, "ERROR: history reset error history can not be a pointer to NULL\r\n");
        fflush(stderr);
    } else {
        h->idx = h->len;
    }
}

history_t historyInit()
{
    history_t h = malloc(sizeof(*h));
    h->maxDim = INITIAL_DIM;
    h->history = malloc(sizeof(*(h->history)) * INITIAL_DIM);
    h->len = 0;
    h->idx = 0;
    return h;
}
void historyFree(history_t h)
{
    if (h == NULL) {
        fprintf(stderr, "ERROR: history reset error history can not be a pointer to NULL\r\n");
        fflush(stderr);
    } else {
        for (size_t i = 0; i < h->len; i++) {
            free(h->history[i]);
        }
        free(h->history);
        free(h);
    }
}

char* historyGetPrevious(history_t h)
{
    char* str = malloc(sizeof(*str) * MAX_STR_LEN);
    if (h->idx - 1 < h->maxDim) {
        h->idx--;
        memccpy(str, h->history[h->idx], strlen(h->history[h->idx]), MAX_STR_LEN);
    } else {
        str = NULL;
    }
    return str;
}

char* historyGetNext(history_t h)
{
    char* str = malloc(sizeof(*str) * MAX_STR_LEN);
    if (h->idx + 1 < h->len) {
        h->idx++;
        memccpy(str, h->history[h->idx], strlen(h->history[h->idx]), MAX_STR_LEN);
    } else {
        str = NULL;
    }
    return str;
}

static int8_t historyRealloc(history_t h)
{
    h->history = realloc(h->history, h->maxDim * 2);
    if (h->history == NULL)
        return 1;
    return 0;
}
