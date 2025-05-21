//
// Created by must9 on 22.05.2025
//

#include <stdio.h>
#include <malloc.h>

#include "cards.h"
#include "hands.h"

char* return_hand_type(int type) {
    char* s = malloc(16);
    const char* rank_names[] = { "Unbekannt", "High Card", "Paar", "Trips", "Quads", "Straße", "Zweier Straße" };
    sprintf(s, "%s", rank_names[type]);
    return s;
}

int get_hand_type(Card *hand, int size) {

    if (size == 1) return HIGH;
    if (size == 2 && hand[0].rank == hand[1].rank) return PAIR;
    if (size == 3 && hand[0].rank == hand[1].rank && hand[1].rank == hand[2].rank) return TRIPS;
    if (size == 4 && hand[0].rank == hand[1].rank && hand[1].rank == hand[2].rank && hand[2].rank == hand[3].rank) return QUADS;

    // straight needs atleast 3 cards
    if (size >= 3) {
        int is_straight = 1;
        for (int i = 0; i < size - 1; i++) {
            if (hand[i].rank + 1 != hand[i+1].rank) {
                is_straight = 0;
                break;
            }
        }
        if (is_straight) return STRASSE;
    }

    // two pair straight needs at least 6 cards and be even
    if (size >= 6 && size % 2 == 0) {
        int valid = 1;
        for (int i = 0; i < size; i += 2) {
            // is the neighboring card the same suit?
            if (hand[i].rank != hand[i+1].rank) {
                valid = 0;
                break;
            }
            if (i < size - 2 && hand[i].rank + 1 != hand[i+2].rank) {
                // is the second next card lesser rank?
                valid = 0;
                break;
            }
        }
        if (valid) return ZWEIER_STRASSE;
    }

    return INVALID;
}

int is_valid_hand(Card* hand, int size) {
    return get_hand_type(hand, size) != INVALID;
}
