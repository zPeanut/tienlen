//
// Created by peanut on 12.05.2025
//

#define NUM_CARDS 52
#define NUM_SUITS 4
#define NUM_PLAYERS 4
#define HAND_SIZE 13

#define S_PIK "♠"
#define S_KREUZ "♣"
#define S_KARO "♦"
#define S_HERZ "♥"

#define WHITE 1
#define BLUE 2
#define YELLOW 3
#define RED 4
#define CYAN 5

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