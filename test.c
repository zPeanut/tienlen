//
// Created by must9 on 12/05/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cards.c"
#include "ncurses.h"
#include <string.h>
#include <locale.h>
#include <unistd.h>

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
            deck -> cards[i].suit = j;
            deck -> cards[i].rank = k;
            i++;
        }
    }
    deck -> index = 0;
}

void shuffle_deck(Deck *deck) {
    srand(time(NULL));
    for (int i = NUM_CARDS - 1; i > 0; i--) {
        int j = rand() % i + 1 ;
        Card temp = deck -> cards[i];
        deck -> cards[i] = deck -> cards[j];
        deck -> cards[j] = temp;
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

char* return_card(Card card) {
    char *s = malloc(8);
    const char *suit_names[] = {"♠", "♣", "♦", "♥"};
    const char *rank_names[] = {"3", "4", "5", "6", "7", "8", "9", "10","J", "Q", "K", "A", "2"};
    sprintf(s, "%s%s", rank_names[card.rank], suit_names[card.suit]);
    return s;
}

void draw_hand(WINDOW *win, int y, int x, int loop_limit, Card *player_deck, int highlight, int *selected_cards) {

    for (int i = 0; i < loop_limit; i++) {
        char *s = return_card(player_deck[i]);

        if(strstr(s, PIK) != NULL) wattron(win, COLOR_PAIR(WHITE));
        if(strstr(s, KREUZ) != NULL) wattron(win, COLOR_PAIR(BLUE));
        if(strstr(s, KARO) != NULL) wattron(win, COLOR_PAIR(YELLOW));
        if(strstr(s, HERZ) != NULL) wattron(win, COLOR_PAIR(RED));
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

void play_cards(Card* cards) {



}

int main() {
    setlocale(LC_ALL, "");

    // init screen
    initscr();
    start_color();
    noecho();
    cbreak();
    curs_set(0);

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    // hand window
    WINDOW *win = newwin(0, x_max - 10, y_max - 7, 5);
    box(win, 0, 0);

    // played cards
    WINDOW *cards = newwin(y_max - 7, x_max - 10, 0, 5);
    box(cards, 0, 0);

    // connected users
    WINDOW *user_section_box = newwin(y_max - 7, x_max - 10, 0, 5);
    box(user_section_box, 0, 0);

    // draw vertical line for connected users
    int line_x = 3 * ((x_max - 10) / 4);
    for (int y = 1; y < (y_max - 7) - 1; y++) {
        mvwaddch(user_section_box, y, line_x, ACS_VLINE);
    }

    char *underline[30];
    for (int i = 0; i < x_max - 8; i++) {
        strcat((char *) underline, " ");
    }
    mvwprintw(user_section_box, 2, line_x + 2, "Connected Users:");
    wattron(user_section_box, A_UNDERLINE);

    mvwprintw(user_section_box, 3, line_x + 2, "%s", (char *) underline);
    wattroff(user_section_box, A_UNDERLINE);
    mvwprintw(user_section_box, 5, line_x + 2, "User1");
    mvwprintw(user_section_box, 7, line_x + 2, "User2");
    mvwprintw(user_section_box, 9, line_x + 2, "User3");
    mvwprintw(user_section_box, 11, line_x + 2, "User4");

    int win_height, win_width;
    getmaxyx(win, win_height, win_width);

    // check for colors
    if (!has_colors()) {
        char* s1 = "-----------------------------------------------";
        char* s2 = "Error! Your terminal does not support color!";
        mvprintw(11 + (win_height / 2), (int) (4 + (win_width - strlen(s1)) / 2), "%s", s1);
        mvprintw(12 + (win_height / 2), (int) (4 + (win_width - strlen(s2)) / 2), "%s", s2);
        mvprintw(13 + (win_height / 2), (int) (4 + (win_width - strlen(s1)) / 2), "%s", s1);
        getch();
        move(0,0);
        endwin();
        return 0;
    }

    // initialize colors for cards
    init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);     // spades
    init_pair(BLUE, COLOR_CYAN, COLOR_BLACK);       // clubs
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);   // diamonds
    init_pair(RED, COLOR_RED, COLOR_BLACK);         // hearts

    keypad(win, true);

    // init deck
    Deck *deck = malloc(sizeof (*deck));
    init_deck(deck);
    shuffle_deck(deck);


    // max hand size (always 13, even if fewer than 4 players are connected)
    int hand_size = 13;

    // choice = key input
    // flag = check if animation already played
    int choice, flag = 0;

    // highlight current selected value
    int highlight = 0;

    // total length of hand (used for centering)
    int total_len = 0;

    // array of flags - checks if card at index is highlighted to be played
    int selected_cards[hand_size];

    // current hand
    Card player_deck[hand_size];

    // played hand (on turn)
    Card played_hand[hand_size];


    // give card to player
    for (int i = 0; i < hand_size; i++) {
        selected_cards[i] = 0;
        player_deck[i] = deck->cards[i];

        char* s = return_card(deck->cards[i]);
        total_len += (int) strlen(s);
        if (i < hand_size - 1) total_len += 2;
        free(s);

    }

    // sort cards by rank
    qsort(player_deck, hand_size, sizeof(Card), compare_by_rank);

    while(1) {
        int x;
        int y = win_height / 2;

        // --- BEGIN ANIMATION ---
        if (!flag) {
            for (int i = 0; i < hand_size; i++) {

                total_len = 0;
                for (int j = 0; j <= i; j++) {
                    char *s = return_card(player_deck[j]);
                    total_len += (int) strlen(s);
                    free(s);
                    if (j < i) total_len += 2;
                }

                mvwhline(win, y, 2, ' ', win_width - 10);
                x = (win_width - total_len) / 2;

                draw_hand(win, y, x, i + 1, player_deck, highlight, selected_cards);

                wrefresh(win);
                wrefresh(cards);
                wrefresh(user_section_box);
                usleep(100 * 1000);
            }
            flag = 1;
        }
        // --- END ANIMATION ---

        // --- BEGIN DISPLAY ---
        mvwhline(win, y, 2, ' ', win_width - 10);

        total_len = 0;
        for (int j = 0; j < hand_size; j++) {

            char *s = return_card(player_deck[j]);
            total_len += (int)strlen(s);
            free(s);
            if (j < hand_size) total_len += 2;
        }

        x = (win_width - total_len) / 2;
        draw_hand(win, y, x, hand_size, player_deck, highlight, selected_cards);

        choice = wgetch(win);

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
                selected_cards[highlight] = !selected_cards[highlight];
                break;
            case KEY_UP: {
                    int new_index = 0;
                    for (int i = 0; i < hand_size; i++) {
                        if (selected_cards[i]) {
                            played_hand[i] = player_deck[i];
                        } else {
                            player_deck[new_index] = player_deck[i];
                            new_index++;
                        }
                        selected_cards[i] = 0;
                    }
                    hand_size = new_index;



                    memset(selected_cards, 0, sizeof (int) * hand_size);
                }
                break;
            default:
                goto end;
        }
        /// --- END DISPLAY ---
    }

    end:
        // free
        endwin();
        free(deck);
        return 0;
}
