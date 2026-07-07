typedef struct history* history_t;

void historyReset(history_t history);
void historyPush(history_t h, char* str);
void historyFree(history_t h);
char* getPrevious(history_t h);
char* historyGetPrevious(history_t h);
char* historyGetNext(history_t h);
history_t historyInit();
