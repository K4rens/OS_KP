#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "common.h"

typedef struct {
    char login[32];
    HANDLE hPipe;
    int game_id;
    int is_active; //клиент подключен - 0/1
} ClientContext;   //инфа о клиенте подключенном

typedef struct {
    char name[32];
    char guest_login[32]; // кто приглашен
    ClientContext *player1;
    ClientContext *player2;
    char board1[BOARD_SIZE][BOARD_SIZE];
    char board2[BOARD_SIZE][BOARD_SIZE];
    int current_turn;
    int p1_ships;
    int p2_ships;
    int is_running;
} GameSession;

ClientContext clients[10];
GameSession games[5];
CRITICAL_SECTION cs;

void init_board(char board[BOARD_SIZE][BOARD_SIZE], int *ships) {
    *ships = 0;
    memset(board, '~', BOARD_SIZE * BOARD_SIZE); //заполняем ячейки
    for(int i=0; i<3; i++) {
        int r = rand() % BOARD_SIZE, c = rand() % BOARD_SIZE;
        if(board[r][c] == '~') { board[r][c] = 'S'; (*ships)++; }
        else i--;
    }
}

unsigned __stdcall ClientHandler(void* pArgs) { //передается контекст
    int idx = *(int*)pArgs;
    ClientContext *me = &clients[idx];
    Message req, res;
    DWORD cb;

    // логин
    if(ReadFile(me->hPipe, &req, sizeof(Message), &cb, NULL)) {
        // дескриптор канала, адрес струк для заполнения, сколько байт читать, 
        strcpy(me->login, req.text); // копируем логин в структуру клиента
        printf("logged in: %s\n", me->login);
    }

    while(ReadFile(me->hPipe, &req, sizeof(Message), &cb, NULL)) {
        EnterCriticalSection(&cs); //вход в крит сек
        memset(&res, 0, sizeof(Message));
        res.type = MSG_ERROR; //ошибка по умол, при успехе - перезапишится

        if(req.type == MSG_INVITE) {
            // логика приглашения: создаем игру и записываем туда гостя
            int g_idx = -1; //не найдено 
            for(int i=0; i<5; i++) if(!games[i].is_running) { g_idx = i; break; }
            
            if(g_idx != -1) {
                games[g_idx].is_running = 1; //активируем игру
                games[g_idx].player1 = me; //первый игрок - создатель
                games[g_idx].player2 = NULL;
                strcpy(games[g_idx].name, req.text); // имя игры
                strcpy(games[g_idx].guest_login, req.target_user); // кого ждем
                me->game_id = g_idx;
                init_board(games[g_idx].board1, &games[g_idx].p1_ships);
                
                res.type = MSG_OK;
                sprintf(res.text, "Game '%s' created. Waiting for %s...", req.text, req.target_user);
            } else strcpy(res.text, "Server games limit reached.");
        }
        else if(req.type == MSG_CHECK_INVITE) {
            // проверяем, есть ли игра, где этот юзер записан как гость
            res.type = MSG_OK;
            strcpy(res.text, "No active invites.");
            for(int i=0; i<5; i++) {
                if(games[i].is_running && strcmp(games[i].guest_login, me->login) == 0 && !games[i].player2) {
                    sprintf(res.text, "You are invited to game: %s", games[i].name);
                    break;
                }
            }
        }
        else if(req.type == MSG_JOIN_GAME) {
            for(int i=0; i<5; i++) {
                if(games[i].is_running && strcmp(games[i].name, req.text) == 0) {
                    games[i].player2 = me;
                    me->game_id = i;
                    init_board(games[i].board2, &games[i].p2_ships);
                    games[i].current_turn = 1;
                    res.type = MSG_OK;
                    break;
                }
            }
            if(res.type != MSG_OK) strcpy(res.text, "Game not found.");
        }
        else if(req.type == MSG_WAIT) {
            if(me->game_id != -1) {
                GameSession *g = &games[me->game_id];
                if(g->player1 && g->player2) {
                    int p_num = (me == g->player1) ? 1 : 2;
                    if(g->current_turn == p_num) {
                        res.type = MSG_OK;
                        memcpy(res.board, (p_num == 1) ? g->board2 : g->board1, sizeof(res.board));
                    } else res.type = MSG_WAIT;
                } else res.type = MSG_WAIT;
            }
        }
        else if(req.type == MSG_SHOOT) {
            GameSession *g = &games[me->game_id];
            int is_p1 = (me == g->player1);
            char (*t_brd)[5] = is_p1 ? g->board2 : g->board1;
            int *t_ships = is_p1 ? &g->p2_ships : &g->p1_ships;

            if(t_brd[req.row][req.col] == 'S') {
                t_brd[req.row][req.col] = 'X';
                (*t_ships)--;
                if(*t_ships <= 0) { res.type = MSG_GAMEOVER; g->is_running = 0; }
                else res.type = MSG_UPDATE;
                strcpy(res.text, "HIT!");
            } else {
                t_brd[req.row][req.col] = '*';
                g->current_turn = is_p1 ? 2 : 1;
                res.type = MSG_UPDATE;
                strcpy(res.text, "MISS!");
            }
        }
        
        LeaveCriticalSection(&cs);
        WriteFile(me->hPipe, &res, sizeof(Message), &cb, NULL);
        if(res.type == MSG_GAMEOVER) break;
    }
    me->is_active = 0;
    CloseHandle(me->hPipe);
    return 0;
}

int main() {
    InitializeCriticalSection(&cs);
    srand(GetTickCount());
    printf("Battleship Server Running...\n");
    while(1) {
        HANDLE h = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 10, BUF_SIZE, BUF_SIZE, 0, NULL);
        //имя, доступ, поведение пайапа, макс колво, размер буферов, деф деф
        if(ConnectNamedPipe(h, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            int found = -1;
            EnterCriticalSection(&cs);
            for(int i=0; i<10; i++) if(!clients[i].is_active) { found = i; break; }
            if(found != -1) {
                clients[found].is_active = 1; clients[found].hPipe = h; //закрепляем за слотом пайп
                int *p = malloc(sizeof(int)); 
                *p = found;
                _beginthreadex(NULL, 0, ClientHandler, p, 0, NULL); //деф деф так где начинается поток контекст 
            }
            LeaveCriticalSection(&cs);
        }
    }
    return 0;
}