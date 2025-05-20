//
// Created by peanut on 12.05.2025
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <locale.h>
#include <errno.h>
#include <ncurses.h>

#include "cards.h"
#include "connect_info.h"

void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, const int *selected_cards) {
    for (int i = 0; i < loop_limit; i++) {

        char* s = return_card(player_deck[i]);
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
        switch (so_error) {
            case ECONNREFUSED:
                printf("Connection to server has failed! Server is not running.\n");
                break;
            default:
                printf("Connection to server has failed! Error code (%d)", so_error);
        }
        return -1;
    }
    fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);
    return 0;
}


int parse_names(char* buffer, char players[NUM_PLAYERS][MAX_NAME_LENGTH]) {
    static char prev_players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    char temp_players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    int players_changed = 0;

    char *token = strchr(buffer, ':');
    if (token != NULL) {
        token++;

        token = strtok(token, ",");
        for (int i = 0; i < NUM_PLAYERS && token; i++) {
            strncpy(temp_players[i], token, MAX_NAME_LENGTH - 1);
            temp_players[i][MAX_NAME_LENGTH - 1] = '\0';
            temp_players[i][strcspn(temp_players[i], "\n")] = '\0'; // replace message delimiter '\n' with null termination to avoid buffer overflow
            token = strtok(NULL, ",");
        }
    }

    // compare with previous state
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (strcmp(players[i], temp_players[i]) != 0) {
            players_changed = 1;
            break;
        }
    }

    if (players_changed) {
        // update both current and previous arrays
        memcpy(prev_players, temp_players, sizeof(prev_players));
        memcpy(players, temp_players, sizeof(temp_players));
    }

    return players_changed;
}

char* get_client_ip() {
    printf("Server IP:\n");
    printf("-> ");

    char client_ip[100];
    fgets(client_ip, 100, stdin);
    client_ip[strcspn(client_ip, "\n")] = 0;

    if (client_ip[0] == '\0') {
        strcpy(client_ip, DEFAULT_IP);
    }

    unsigned long str_size = strlen(client_ip) + 1;
    char* ip = malloc(str_size);
    strncpy(ip, client_ip, str_size);
    return ip;
}

int get_client_port() {
    printf("Port:\n");
    printf("-> ");

    char client_port[100];
    fgets(client_port, 100, stdin);
    client_port[strcspn(client_port, "\n")] = 0;
    if (client_port[0] == '\0') {
        snprintf(client_port, 7, "%i", DEFAULT_PORT);
    }
    int port = (int) atol(client_port);
    return port;
}

char* get_client_name() {
    char client_name[MAX_NAME_LENGTH];
    do {
        memset(client_name, 0, sizeof(client_name));
        printf("Username:\n");
        printf("-> ");
        fgets(client_name, MAX_NAME_LENGTH, stdin);
        if (strpbrk(client_name, ",;")) {
            printf("Invalid characters (`,` or `;`) in name.\n");
            continue;
        }
        client_name[strcspn(client_name, "\n")] = '\0';
    } while (client_name[0] == 0);

    unsigned long str_size = strlen(client_name) + 1;
    char* name = malloc(str_size);
    strncpy(name, client_name, str_size);
    return name;
}

int setup_connection(int timeout, char (*players)[MAX_NAME_LENGTH], int *max_players) {

    char* ip = get_client_ip();
    int port = get_client_port();

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    printf("Trying to connect to %s:%i...\n", ip, port);
    if ((connect_timeout(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr), timeout) == -1)) {
        close(sock);
        exit(1);
    }

    printf("Connected to %s:%d\n", ip, port);

    char* name = get_client_name();
    write(sock, name, strlen(name));

    // build initial userlist from server
    char recv_buffer[256] = { 0 };
    ssize_t received = read(sock, recv_buffer, sizeof(recv_buffer) - 1);
    recv_buffer[received] = '\0';
    if (received > 0) {
        // parse player names with comma
        memset(players, 0, sizeof(*players));
        parse_names(recv_buffer, players);
    }

    char get_max_player_buf[20] = { 0 };
    read(sock, get_max_player_buf, sizeof(get_max_player_buf));
    char* colon = strchr(get_max_player_buf, ':');
    if (colon != NULL) {
        *max_players = (atoi(colon + 1));
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK); // non blocking mode

    free(ip);
    free(name);
    return sock;
}

int setup_ncurses_ui() {
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
        return 1;
    }

    // color initialization
    init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);     // spades
    init_pair(BLUE, COLOR_CYAN, COLOR_BLACK);       // clubs
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);   // diamonds
    init_pair(RED, COLOR_RED, COLOR_BLACK);         // hearts

    return 0;
}


int main() {
    setlocale(LC_ALL, "");

    // ---- BEGIN VARIABLE DECLARATION ----
    char players[NUM_PLAYERS][MAX_NAME_LENGTH] = { 0 };
    int max_players;

    int all_players_connected = 0;
    int any_selected = 0;           // check if any cards are even selected
    int choice;                     // choice = key input
    int flag = 0;                   // flag = check if animation already played
    int hand_size = HAND_SIZE;      // max win_hand size (always 13, even if fewer than 4 players are connected)
    int highlight = 0;              // highlight flag for selected card
    int played = 0;                 // keep track if it's your turn or not
    int played_hand_size = 0;
    static char recv_buffer[4096] = { 0 };          // global buffer to collect data
    static size_t recv_buffer_len = 0;              // track buffer length
    int selected_cards[hand_size];                  // array of flags - checks if card at index is highlighted to be played
    int sock = setup_connection(8, players, &max_players); // setup client connection to server
    int total_len;                                  // total length of win_hand (used for centering)
    int turn = 1;                                   // turn check flag
    int waiting_dots_index = 0;

    Card player_deck[hand_size]; // current win_hand
    Card played_hand[hand_size]; // played win_hand (on turn)
    memset(played_hand, 0, hand_size * sizeof(int)); // need to init array to NULL to check if win_server are inside it
    // ---- END VARIABLE DECLARATION ----


    // ---- BEGIN UI INIT ----
    setup_ncurses_ui();

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    int height = y_max - 7;
    int width = x_max - 10;
    WINDOW *win_hand = newwin(0, width, height, 5); // client hand
    WINDOW *win_server = newwin(height, width, 0, 5); // server messages
    WINDOW *win_user = newwin(height, width, 0, 5); // connected users
    box(win_hand, 0, 0);
    box(win_server, 0, 0);
    box(win_user, 0, 0);

    keypad(win_hand, true);
    nodelay(win_hand, true); // make input non-blocking

    int win_height, win_width;
    getmaxyx(win_hand, win_height, win_width);

    // initial rendering of userlist
    int line_x = 3 * (width / 4); // 3/4th of the screen
    mvwprintw(win_user, 2, line_x + 2, "Connected Users:");

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if(strlen(players[i]) > 0) {
            mvwprintw(win_user, 5 + i * 2, line_x + 2, "%s", players[i]);
            player_count++;
        }
        all_players_connected = (player_count == max_players);  // check if with current connection, enough players are connected
    }

    wrefresh(win_user);
    wrefresh(win_hand);
    // ---- END UI INIT ----


    // game loop
    while (1) {

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds); // monitor socket

        // --- BEGIN RECEIVING DATA ---
        char buffer[256];
        ssize_t recv_loop = read(sock, buffer, sizeof(buffer) - 1);
        if (recv_loop > 1) {
            recv_buffer[recv_buffer_len] = '\0';

            memcpy(recv_buffer + recv_buffer_len, buffer, recv_loop); // append to global buffer
            recv_buffer_len += recv_loop;

        } else if (recv_loop == 0) {
            goto end;
        }

        char *parsed_message_end;
        while ((parsed_message_end = strchr(recv_buffer, '\n')) != NULL) { // '\n' is server delimiter between message types

            *parsed_message_end = '\0';

            // individual message parsing
            if (strstr(recv_buffer, "PLAYERS:")) {
                if (parse_names(recv_buffer, players)) { // only update if players changed

                    player_count = 0;
                    for (int i = 0; i < max_players; i++) {
                        if (strlen(players[i]) > 0) player_count++; // calculate new player count
                    }
                }

                for (int i = line_x + 2; i < width - 2; i++) {
                    for (int j = 0; j < max_players; j++) {
                        mvwaddch(win_user, 5 + j * 2, i, ' '); // clear old users
                        mvwprintw(win_user, 5 + i * 2, line_x + 2, "%s", players[i]);
                    }
                }
                all_players_connected = (player_count == max_players);
                wnoutrefresh(win_user); // queue for refresh

            } else if (strstr(recv_buffer, "DEAL:")) {
                memset(selected_cards, 0, sizeof(selected_cards)); // reset
                char *token = strtok(recv_buffer + 5, ";"); // skip prefix

                for (int i = 0; i < hand_size && token; i++) {
                    sscanf(token, "%d,%d", (int *) &player_deck[i].suit, (int *) &player_deck[i].rank);
                    token = strtok(NULL, ";");
                }
                qsort(player_deck, hand_size, sizeof(Card), compare_by_rank); // sort win_server by rank
                flag = 0; // enable animation
            }

            else if (strstr(recv_buffer, "AMOUNT:")) {
                char* colon = strchr(recv_buffer, ':');
                if (colon) {
                    max_players = atoi(colon + 1);
                }
            }

            // remove parsed message from buffer
            size_t remaining = recv_buffer_len - (parsed_message_end - recv_buffer + 1);
            memmove(recv_buffer, parsed_message_end + 1, remaining);
            recv_buffer_len = remaining;
            recv_buffer[recv_buffer_len] = '\0';
        }
        // --- END RECEIVING DATA ---


        // --- BEGIN UI SECTION ---
        int x;
        int y = win_height / 2;

        // win_user loop
        mvwprintw(win_user, 2, line_x + 2, "Connected Users:");
        for (int i = 1; i < height - 1; i++) {
            mvwaddch(win_user, i, line_x, ACS_VLINE); // draw vertical line for connected users
        }

        for (int i = line_x + 2; i < width - 2; i++) {
            mvwaddch(win_user, 3, i, ACS_HLINE); // draw underline
        }

        for (int i = 0; i < max_players; i++) {
            if (strlen(players[i]) > 0) mvwprintw(win_user, 5 + i * 2, line_x + 2, "%s", players[i]);
        }

        // waiting room
        if (!all_players_connected) {
            flag = 0; // ensure animation plays again when reconnected
            char* waiting_msg = " Waiting for players to connect";
            char* dots[] = { " ", ". ", ".. ", "... "};
            int num_frames = 4;

            char full_msg[50];
            snprintf(full_msg, sizeof(full_msg), "%s%s", waiting_msg, dots[waiting_dots_index % num_frames]);

            mvwhline(win_hand, win_height / 2, 2, ' ', win_width - 10);
            mvwprintw(win_hand, win_height / 2, (int) (win_width - strlen(full_msg) + (waiting_dots_index % num_frames) - 1) / 2, "%s", full_msg);
            mvwprintw(win_hand, win_height / 2 + 1, (win_width - 8) / 2, "(%d/%d)", player_count, max_players);

            waiting_dots_index++;
            wrefresh(win_hand);
            wrefresh(win_user);
            napms(1000); // add 100ms delay to reduce cpu usage
            doupdate();
            continue;
        }

        // -- animation begin --
        if (!flag) {
            werase(win_hand);
            box(win_hand, 0, 0);

            for (int i = 0; i < hand_size; i++) {

                // calculate total length for centering
                total_len = 0;
                for (int j = 0; j <= i; j++) {
                    char* s = return_card(player_deck[j]);
                    total_len += (int) strlen(s);
                    if (j < i) total_len += 2;
                    free(s);
                }

                x = (win_width - total_len) / 2;

                mvwhline(win_hand, y, 2, ' ', win_width - 10);
                draw_hand(win_hand, y, x, i + 1, player_deck, highlight, selected_cards);

                wrefresh(win_hand);
                wrefresh(win_server);
                wrefresh(win_user);
                usleep(100 * 1000);
            }
            flag = 1;
        }
        // -- animation end --

        mvwhline(win_hand, y, 2, ' ', win_width - 10);

        // calculate total length again
        total_len = 0;
        for (int j = 0; j < hand_size; j++) {
            char* s = return_card(player_deck[j]);
            total_len += (int) strlen(s);
            if (j < hand_size - 1) total_len += 2;
            free(s);
        }

        x = (win_width - total_len) / 2;
        draw_hand(win_hand, y, x, hand_size, player_deck, highlight, selected_cards);
        // --- END UI SECTION ---


        // --- BEGIN GAME LOGIC ---
        if (played && turn) {
            int x_pos = 14; // starting x pos after "you played: "
            mvwhline(win_server, 4, 2, ' ', line_x - 2);

            if (any_selected) {
                mvwprintw(win_server, 4, 2, "You played: ");
                for (int i = 0; i < played_hand_size; i++) {
                    char* msg = return_card(played_hand[i]);

                    if (strstr(msg, PIK)) wattron(win_server, COLOR_PAIR(WHITE));
                    else if (strstr(msg, KREUZ)) wattron(win_server, COLOR_PAIR(BLUE));
                    else if (strstr(msg, KARO)) wattron(win_server, COLOR_PAIR(YELLOW));
                    else if (strstr(msg, HERZ)) wattron(win_server, COLOR_PAIR(RED));

                    mvwprintw(win_server, 4, x_pos, "%s", msg);
                    x_pos += (int) strlen(msg);

                    wattroff(win_server, COLOR_PAIR(WHITE));
                    wattroff(win_server, COLOR_PAIR(BLUE));
                    wattroff(win_server, COLOR_PAIR(YELLOW));
                    wattroff(win_server, COLOR_PAIR(RED));

                    free(msg);
                }

            } else {
                mvwprintw(win_server, 4, 2, "You passed.");
                turn = 0;
            }

            memset(played_hand, 0, hand_size * sizeof(int));
            played_hand_size = 0;
            played = 0;
            any_selected = 0;
            wrefresh(win_server);
        }
        // --- END GAME LOGIC ---


        // --- PARSE INPUT ---
        choice = wgetch(win_hand);
        if (choice != ERR) {
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
        }
        doupdate();
    }

    end:
    // exit program
    move(0,0);
    endwin();
    close(sock);
    printf("Server closed!\n");
    return 0;
}
