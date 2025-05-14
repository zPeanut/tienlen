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

Card deal_card(Deck *deck) {
    if (deck -> index >= NUM_CARDS) {
        exit(1);
    }
    return deck -> cards[deck -> index++];
}

void print_card(Card card) {
    const char *suit_names[] = {"♠", "♣", "♦", "♥"};
    const char *rank_names[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10","J", "Q", "K", "A"};
    printf("%s%s\n", rank_names[card.rank], suit_names[card.suit]);
}

char* return_card(Card card) {
    char *s = malloc(8);
    const char *suit_names[] = {"♠", "♣", "♦", "♥"};
    const char *rank_names[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10","J", "Q", "K", "A"};
    sprintf(s, "%s%s", rank_names[card.rank], suit_names[card.suit]);
    return s;
}

int main() {
    setlocale(LC_ALL, "");

    // init screen
    initscr();
    noecho();
    cbreak();
    curs_set(0);

    int y_max, x_max;
    getmaxyx(stdscr, y_max, x_max);

    // window
    WINDOW *win = newwin(0, x_max - 10, y_max - 7, 5);
    box(win, 0, 0);
    refresh();
    wrefresh(win);
    keypad(win, true);

    Deck *deck = malloc(sizeof (*deck));
    init_deck(deck);
    shuffle_deck(deck);

    int hand_size = 10;
    int choice, flag = 0;
    int highlight = 0;
    int selected = 0;
    int selected_cards[hand_size];

    for (int i = 0; i < hand_size; i++) {
        selected_cards[i] = 0;
    }

    while(1) {

        Card player_deck[hand_size];
        int total_len = 0;


        for (int i = 0; i < hand_size; i++) {
            player_deck[i] = deck->cards[i];
            char* s = return_card(deck->cards[i]);
            total_len += (int) strlen(s);

            if (i < hand_size - 1) total_len += 2;
            free(s);
        }

        int win_height, win_width;
        getmaxyx(win, win_height, win_width);

        int x;
        int y = win_height / 2;

        // animation
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

                for (int j = 0; j <= i; j++) {
                    char *s = return_card(player_deck[j]);
                    mvwprintw(win, y, x, "%s", s);
                    x += (int) strlen(s) + 2;
                    free(s);
                }
                wrefresh(win);
                usleep(100 * 1000);
            }
            flag = 1;
        }

        mvwhline(win, y, 2, ' ', win_width - 10);
        x = (win_width - total_len) / 2;

        for (int i = 0; i < hand_size; i++) {
            char *s = return_card(player_deck[i]);

            if (i == highlight) wattron(win, A_REVERSE);
            if (selected_cards[i]) wattron(win, A_INVIS);
            mvwprintw(win, y, x, "%s", s);
            wattroff(win, A_REVERSE);
            wattroff(win, A_INVIS);

            x += (int) strlen(s) + 2; // add 2 spaces between cards
            free(s);
        }

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
            case 10:
                selected_cards[highlight] = !selected_cards[highlight];
            default:
                ;
        }
    }

    end:
        // free
        endwin();
        free(deck);
        return 0;
}
