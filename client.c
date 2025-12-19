#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

HANDLE hPipe;

void draw(char b[BOARD_SIZE][BOARD_SIZE]) {
    printf("\n  0 1 2 3 4\n");
    for(int i=0; i<5; i++){
        printf("%d ", i);
        for(int j=0; j<5; j++) printf("%c ", b[i][j] == 'S' ? '~' : b[i][j]);
        printf("\n");
    }
}

void play() {
    Message req, res; DWORD cb;
    while(1) {
        req.type = MSG_WAIT;
        WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);
        ReadFile(hPipe, &res, sizeof(Message), &cb, NULL);
        if(res.type == MSG_OK) {
            draw(res.board);
            printf("Your turn! Enter row and col: ");
            scanf("%d %d", &req.row, &req.col);
            req.type = MSG_SHOOT;
            WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);
            ReadFile(hPipe, &res, sizeof(Message), &cb, NULL);
            printf("Result: %s\n", res.text);
            if(res.type == MSG_GAMEOVER) { printf("VICTORY!\n"); break; }
        } else if(res.type == MSG_GAMEOVER) { printf("DEFEAT!\n"); break; }
        else { printf("."); Sleep(1000); }
    }
}

int main() {
    hPipe = CreateFile(PIPE_NAME, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hPipe == INVALID_HANDLE_VALUE) return 1;

    Message req, res; DWORD cb;
    printf("Login: "); scanf("%s", req.text);
    req.type = MSG_LOGIN;
    WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);

    while(1) {
        printf("\n1. Invite & Create | 2. Join | 3. Check Invites | 4. Exit\n> ");
        int c; scanf("%d", &c);
        if(c == 1) {
            printf("Game name: "); scanf("%s", req.text);
            printf("Invite user: "); scanf("%s", req.target_user);
            req.type = MSG_INVITE;
            WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);
            ReadFile(hPipe, &res, sizeof(Message), &cb, NULL);
            printf("%s\n", res.text); play();
        } else if(c == 2) {
            printf("Game name: "); scanf("%s", req.text);
            req.type = MSG_JOIN_GAME;
            WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);
            ReadFile(hPipe, &res, sizeof(Message), &cb, NULL);
            if(res.type == MSG_OK) play(); else printf("%s\n", res.text);
        } else if(c == 3) {
            req.type = MSG_CHECK_INVITE;
            WriteFile(hPipe, &req, sizeof(Message), &cb, NULL);
            ReadFile(hPipe, &res, sizeof(Message), &cb, NULL);
            printf("Alert: %s\n", res.text);
        } else break;
    }
    CloseHandle(hPipe);
    return 0;
}