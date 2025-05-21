//
// Created by peanut on 12.05.2025
//

#include "macros.h"

typedef enum {
    PIK, KREUZ, KARO, HERZ
} Suit;

typedef enum {
    DREI, VIER, FÜNF, SECHS, SIEBEN, ACHT, NEUN, ZEHN, BUBE, DAME, KÖNIG, ASS, ZWEI
} Rank;

typedef struct {
    Suit suit;
    Rank rank;
} Card;

typedef struct {
    Card cards[NUM_CARDS];
    int index; // top card
} Deck;

void init_deck(Deck *deck);
void shuffle_deck(Deck *deck);
int compare_by_rank(const void *a, const void *b);
char* return_card(Card card);