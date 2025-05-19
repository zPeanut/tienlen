//
// Created by peanut on 12.05.2025
//

#define NUM_CARDS 52
#define NUM_SUITS 4
#define NUM_PLAYERS 4
#define HAND_SIZE 13

typedef enum {
    PIK, KREUZ, KARO, HERZ
} Suit;

typedef enum {
    ZWEI, DREI, VIER, FÜNF, SECHS, SIEBEN, ACHT, NEUN, ZEHN, BUBE, DAME, KÖNIG, ASS
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