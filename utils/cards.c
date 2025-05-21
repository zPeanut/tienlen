//
// Created by peanut on 18.05.2025
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "cards.h"

void init_deck(Deck *deck) {
    int i = 0;
    for (Suit s = PIK; s <= HERZ; s++) {
        for (Rank r = DREI; r <= ZWEI; r++) {
            deck->cards[i].suit = s;
            deck->cards[i].rank = r;
            i++;
        }
    }
    deck->index = 0;
}

// fisher yates algorithm
void shuffle_deck(Deck *deck) {
    srand(time(NULL));
    for (int i = 0; i < NUM_CARDS; i++) {
        int j = i + (rand() % (53 - i));
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

char* return_card(Card card) {
    char* s = malloc(8);
    const char* suit_names[] = {"♠", "♣", "♦", "♥"};
    const char* rank_names[] = {"3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A", "2"};
    sprintf(s, "%s%s", rank_names[card.rank], suit_names[card.suit]);
    return s;
}
