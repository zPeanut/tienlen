//
// Created by peanut on 22.05.2025
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
            if (hand[i].rank + 1 != hand[i + 1].rank) {
                is_straight = 0;
                break;
            }
        }
        if (is_straight) return STRASSE;
    }

    // TODO: this isnt actually a bomb, this is an instant win but ill have to change this later
    if (size == 4) {
        int is_instant_win = 1;
        for (int i = 0; i < size; i++) {
            if (hand[i].rank != ZWEI) {
                is_instant_win = 0;
                break;
            }
        }
        if (is_instant_win) return WIN;
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

int is_valid_hand(Card *hand1, Card *hand2, int size) {
    return (get_hand_type(hand1, size) != INVALID) && (get_hand_type(hand1, size) == get_hand_type(hand2, size));
}

int compare_ranks(Rank rank1, Rank rank2) {
    return rank1 > rank2;
}

int compare_suits(Suit suit1, Suit suit2) {
    return suit1 > suit2;
}

int compare_cards(Card card1, Card card2) {
    if (card1.rank != card2.rank) return compare_ranks(card1.rank, card2.rank);
    return compare_suits(card1.suit, card2.suit);
}

Suit get_max_suit(Card* hand, int size) {
    Suit max = hand[0].suit;
    for (int i = 1; i < size; i++)
        if (hand[i].suit > max) max = hand[i].suit;
    return max;
}

Card get_highest_card(Card* hand, int size) {
    Card highest = hand[0];
    for (int i = 1; i < size; i++)
        if (compare_cards(hand[i], highest))
            highest = hand[i];
    return highest;
}

int is_hand_higher(Card *hand1, Card *hand2, int size) {
    Hand type1 = get_hand_type(hand1, size);
    Hand type2 = get_hand_type(hand2, size);

    if (type1 == BOMB || type2 == BOMB) {
        if (type1 == BOMB && type2 != BOMB) return 1;
        if (type2 == BOMB && type1 != BOMB) return 0;
        return compare_ranks(hand1[0].rank, hand2[0].rank);
    }

    if (type1 != type2) return 0;

    if (type1 == HIGH) {
        return compare_cards(hand1[0], hand2[0]);
    } else if (type1 == PAIR || type1 == TRIPS) {
        if (hand1[0].rank != hand2[0].rank)
            return compare_ranks(hand1[0].rank, hand2[0].rank);
        // Compare highest suit in the group
        return compare_suits(get_max_suit(hand1, size), get_max_suit(hand2, size));
    } else if (type1 == STRASSE) {
        Card highest1 = get_highest_card(hand1, size);
        Card highest2 = get_highest_card(hand2, size);
        return compare_cards(highest1, highest2);
    }

    return 0;
}


