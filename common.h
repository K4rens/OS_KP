#ifndef COMMON_H
#define COMMON_H

#include <windows.h>

#define PIPE_NAME "\\\\.\\pipe\\BattleshipPipe"
#define BUF_SIZE 1024
#define BOARD_SIZE 5

typedef enum {
    MSG_LOGIN, 
    MSG_CREATE_GAME, 
    MSG_JOIN_GAME,
    MSG_INVITE,      // отправить приглашение
    MSG_CHECK_INVITE,// проверить, звали ли меня
    MSG_SHOOT,
    MSG_WAIT,
    MSG_UPDATE,
    MSG_GAMEOVER,
    MSG_ERROR,
    MSG_OK
} MsgType;

typedef struct {
    MsgType type;
    char text[128];        // имя игры / логин / сообщения
    char target_user[32];  // для приглашения: кого зовем
    int row;
    int col;
    char board[BOARD_SIZE][BOARD_SIZE];
} Message;

#endif