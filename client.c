//
// Created by peanut on 12/05/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <locale.h>
#include "cards.c"
#include "errno.h"
#include "ncurses.h"

#define PIK "♠"
#define KREUZ "♣"
#define KARO "♦"
#define HERZ "♥"

#define YELLOW 99
#define WHITE 98
#define BLUE 97
#define RED 96

void init_deck(Deck *deck) {
    int i = 0;
    for (int j = 0; j < NUM_SUITS; j++) {
        for (int k = 0; k < NUM_RANKS; k++) {
            deck->cards[i].suit = j;
            deck->cards[i].rank = k;
            i++;
        }
    }
    deck->index = 0;
}

void shuffle_deck(Deck *deck) {
    srand(time(NULL));
    for (int i = NUM_CARDS - 1; i > 0; i--) {
        int j = rand() % i + 1;
        Card temp = deck->cards[i];
        deck->cards[i] = deck->cards[j];
        deck->cards[j] = temp;
    }
}

int compare_by_rank(const void *a, const void *b) {
    Card *card1 = (Card *) a;
    Card *card2 = (Card *) b;

    if (card1->rank != card2->rank) {
        return (int) (card1->rank - card2->rank);
    }
    return (int) (card1->suit - card2->suit);
}

char *return_card(Card card) {
    char *s = malloc(8);
    const char *suit_names[] = {"♠", "♣", "♦", "♥"};
    const char *rank_names[] = {"3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2"};
    sprintf(s, "%s%s", rank_names[card.rank], suit_names[card.suit]);
    return s;
}

void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, int *selected_cards) {

    for (int i = 0; i < loop_limit; i++) {
        char *s = return_card(player_deck[i]);

        if (strstr(s, PIK) != NULL) wattron(win, COLOR_PAIR(WHITE));
        if (strstr(s, KREUZ) != NULL) wattron(win, COLOR_PAIR(BLUE));
        if (strstr(s, KARO) != NULL) wattron(win, COLOR_PAIR(YELLOW));
        if (strstr(s, HERZ) != NULL) wattron(win, COLOR_PAIR(RED));
        if (i == highlight) wattron(win, A_UNDERLINE);
        if (selected_cards[i]) wattron(win, (A_BLINK | A_REVERSE));

        mvwprintw(win, y, x, "%s", s);

        wattroff(win, A_REVERSE);
        wattroff(win, A_UNDERLINE);
        wattroff(win, A_BLINK);
        wattroff(win, COLOR_PAIR(WHITE));
        wattroff(win, COLOR_PAIR(BLUE));
        wattroff(win, COLOR_PAIR(YELLOW));
        wattroff(win, COLOR_PAIR(RED));

        x += (int) strlen(s) + 2; // add 2 spaces between cards
        free(s);
    }
}

int connect_timeout(int socket, struct sockaddr *address, socklen_t address_length, int timeout) {

    int flags = fcntl(socket, F_GETFL, 0);
    // non block
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    int result = connect(socket, address, address_length);
    if (result == 0) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
        return 0;
    } else if (errno != EINPROGRESS) {
        return -1;
    }

    fd_set writefds;
    struct timeval tv;

    FD_ZERO(&writefds);
    FD_SET(socket, &writefds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    result = select(socket + 1, NULL, &writefds, NULL, &tv);
    if (result <= 0) {
        printf("Connection to server has timed out.\n");
        return -1;
    }

    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

    if (so_error != 0) {
        printf("Connection to server has failed.\n");
        return -1;
    }

    fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
    return 0;
}


int main() {
    setlocale(LC_ALL, "");

    // ---- BEGIN VARIABLE DECLARATION ----
    int hand_size = 13; // max win_hand size (always 13, even if fewer than 4 players are connected)
    int choice, flag = 0; // choice = key input, flag = check if animation already played
    int highlight = 0; // highlight current selected value
    int total_len = 0; // total length of win_hand (used for centering)
    int selected_cards[hand_size]; // array of flags - checks if card at index is highlighted to be played
    int played = 0; // keep track if its your turn or not
    int played_hand_size = 0;
    int any_selected = 0; // check if any win_chat are even played
    int turn = 1; // turn check flag
    char players[4][30] = {0};

    Card player_deck[hand_size]; // current win_hand
    Card played_hand[hand_size]; // played win_hand (on turn)
    // need to init array to NULL to check if win_chat are inside it
    memset(played_hand, 0, hand_size * sizeof(int));
    // ---- END VARIABLE DECLARATION ----


    // ---- BEGIN INIT PHASE ----
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    printf("Server IP:\n");
    printf("-> ");

    char ip[100];
    fgets(ip, 100, stdin);
    ip[strcspn(ip, "\n")] = 0;
    if (ip[0] == '\0') {
        strcpy(ip, "127.0.0.1");
    }

    printf("Port:\n");
    printf("-> ");

    char port[100];
    fgets(port, 100, stdin);
    port[strcspn(port, "\n")] = 0;
    if (port[0] == '\0') {
        strcpy(port, "25565");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    printf("Trying to connect to %s:%s...\n", ip, port);

    if ((connect_timeout(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr), 8) == -1)) {
        exit(1);
    }

    printf("Connected to %s:%s\n", ip, port);



    char name[30];
    do {
        printf("Enter your name?\n");
        printf("-> ");
        fgets(name, 30, stdin);
        name[strcspn(name, "\n")] = 0;
    } while (name[0] == 0);

    send(sock, name, strlen(name), 0);

    for (int i = 0; i < 4; i++) {
        if (players[i] != 0) {
            strcpy(players[i], name);
            break;
        }
    }

    // ---- BEGIN UI-INIT PHASE ----
    initscr();
    start_color();
    noecho();
    cbreak();
    curs_set(0);

    if (!has_colors()) {
        printw("Your terminal doesnt support colors! Session terminated.\n");
        printw("Press any key to continue...");
        getch();
        endwin();
        return 0;
    }

    // deck initialization
    Deck *deck = malloc(sizeof(*deck));
    init_deck(deck);
    shuffle_deck(deck);

    // color initialization
    init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);     // spades
    init_pair(BLUE, COLOR_CYAN, COLOR_BLACK);       // clubs
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);   // diamonds
    init_pair(RED, COLOR_RED, COLOR_BLACK);         // hearts

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    int height = y_max - 7;
    int width = x_max - 10;
    WINDOW *win_hand = newwin(0, width, height, 5); // win_hand window
    WINDOW *win_chat = newwin(height, width, 0, 5); // played win_chat
    WINDOW *win_user = newwin(height, width, 0, 5); // connected users
    box(win_hand, 0, 0);
    box(win_chat, 0, 0);
    box(win_user, 0, 0);
    keypad(win_hand, true);

    int win_height, win_width;
    getmaxyx(win_hand, win_height, win_width);

    // give win_chat to player
    // TODO: this is supposed to be on the server, and every player needs this
    for (int i = 0; i < hand_size; i++) {
        selected_cards[i] = 0;
        player_deck[i] = deck->cards[i];

        char *s = return_card(deck->cards[i]);
        total_len += (int) strlen(s);
        if (i < hand_size - 1) total_len += 2;
        free(s);
    }

    qsort(player_deck, hand_size, sizeof(Card), compare_by_rank); // sort win_chat by rank

    int line_x = 3 * (width / 4); // 3/4th of the screen
    for (int y = 1; y < height - 1; y++) {
        mvwaddch(win_user, y, line_x, ACS_VLINE); // draw vertical line for connected users
    }

    for (int x = line_x + 2; x < width - 2; x++) {
        mvwaddch(win_user, 3, x, ACS_HLINE); // draw underline
    }

    mvwprintw(win_user, 2, line_x + 2, "Connected Users:");

    // ---- END UI-INIT PHASE ----


    int connected = 0;
    // ---- BEGIN GAME LOOP ----
    while (1) {

        int temp_height, temp_width;
        getmaxyx(win_chat, temp_height, temp_width);
        char *waiting_msg = "Waiting for players";
        char *dots[] = {"", ".", "..", "..."};
        int num_frames = sizeof(dots) / sizeof(dots[0]);
        int k = 0;

        while(!connected) {
            char full_msg[50];
            sprintf(full_msg, "%s%s", waiting_msg, dots[k % num_frames]);

            mvwhline(win_chat, temp_height / 2, 1, ' ', (int) (temp_width - strlen(full_msg)) / 4 + 40);
            mvwprintw(win_chat, temp_height / 2, (int) (temp_width - strlen(full_msg + 1)) / 4 + 5, "%s", full_msg);

            // TODO: add actual users here
            for (int i = 0; i < 4; i++) {
                if (players[i] != 0) {
                    mvwprintw(win_user, 5 + i * 2, line_x + 2, "%s", players[i]);
                }
            }

            wrefresh(win_hand);
            wrefresh(win_chat);
            wrefresh(win_user);

            usleep(400 * 1500);
            k++;

            // WAITING ROOM
            /*char buffer[16];
            ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (len > 0) {
                buffer[len] = '\0';
                if (strcmp(buffer, "tien") == 0) {
                    send(sock, "len", 4, 0);
                }
            }*/
        }

        if (connected) {
            // --- BEGIN UI SECTION ---
            int x;
            int y = win_height / 2;

            // -- animation begin --
            if (!flag) {
                for (int i = 0; i < hand_size; i++) {

                    total_len = 0;
                    for (int j = 0; j <= i; j++) {
                        char *s = return_card(player_deck[j]);
                        total_len += (int) strlen(s);
                        free(s);
                        if (j < i) total_len += 2;
                    }

                    mvwhline(win_hand, y, 2, ' ', win_width - 10);
                    x = (win_width - total_len) / 2;

                    draw_hand(win_hand, y, x, i + 1, player_deck, highlight, selected_cards);

                    wrefresh(win_hand);
                    wrefresh(win_chat);
                    wrefresh(win_user);
                    usleep(100 * 1000);
                }
                flag = 1;
            }
            // -- animation end --

            mvwhline(win_hand, y, 2, ' ', win_width - 10);

            total_len = 0;
            for (int j = 0; j < hand_size; j++) {
                char *s = return_card(player_deck[j]);
                total_len += (int) strlen(s);
                free(s);
                if (j < hand_size - 1) total_len += 2;
            }

            x = (win_width - total_len) / 2;
            draw_hand(win_hand, y, x, hand_size, player_deck, highlight, selected_cards);
            // --- END UI SECTION ---


            // --- BEGIN GAME LOGIC ---
            if (played && turn) {
                int x_pos = 14; // starting x pos after "you played: "
                mvwhline(win_chat, 4, 2, ' ', line_x - 2);

                if (any_selected) {
                    mvwprintw(win_chat, 4, 2, "You played: ");
                    for (int i = 0; i < played_hand_size; i++) {
                        char *msg = return_card(played_hand[i]);

                        if (strstr(msg, PIK)) wattron(win_chat, COLOR_PAIR(WHITE));
                        else if (strstr(msg, KREUZ)) wattron(win_chat, COLOR_PAIR(BLUE));
                        else if (strstr(msg, KARO)) wattron(win_chat, COLOR_PAIR(YELLOW));
                        else if (strstr(msg, HERZ)) wattron(win_chat, COLOR_PAIR(RED));

                        mvwprintw(win_chat, 4, x_pos, "%s", msg);
                        x_pos += (int) strlen(msg);

                        wattroff(win_chat, COLOR_PAIR(WHITE));
                        wattroff(win_chat, COLOR_PAIR(BLUE));
                        wattroff(win_chat, COLOR_PAIR(YELLOW));
                        wattroff(win_chat, COLOR_PAIR(RED));

                        free(msg);
                    }

                } else {
                    mvwprintw(win_chat, 4, 2, "You passed.");
                    turn = 0;
                }

                memset(played_hand, 0, hand_size * sizeof(int));
                played_hand_size = 0;
                played = 0;
                any_selected = 0;
                wrefresh(win_chat);
            }
            // --- END GAME LOGIC ---


            // --- BEGIN CONTROLS ---
            choice = wgetch(win_hand);
            switch (choice) {

                case KEY_LEFT:
                    highlight--;
                    if (highlight == -1) highlight = hand_size - 1;
                    break;

                case KEY_RIGHT:
                    highlight++;
                    if (highlight == hand_size) highlight = 0;
                    break;

                case 10: // enter
                case KEY_UP:
                    selected_cards[highlight] = !selected_cards[highlight];
                    break;

                case 32: { // space
                    if (turn) {
                        int new_index = 0;
                        any_selected = 0;

                        for (int i = 0; i < hand_size; i++) {
                            if (selected_cards[i]) {
                                any_selected = 1;
                                played_hand[played_hand_size++] = player_deck[i];
                            } else {
                                player_deck[new_index++] = player_deck[i];
                            }
                            selected_cards[i] = 0;
                        }
                        played = 1;
                        hand_size = new_index;
                        if (highlight > new_index) highlight = new_index - 1;
                        memset(selected_cards, 0, hand_size * sizeof(int));
                    }
                }
                    break;

                default:
                    goto end;
            }
            // --- END CONTROLS ---
        }
    }
    // ---- END GAME LOOP ----

    end:
    // exit program
    endwin();
    free(deck);
    close(sock);
    return 0;
}
