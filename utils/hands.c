//
// Created by peanut on 22.05.2025
//

#include <stdio.h>
#include <malloc.h>

#include "cards.h"
#include "hands.h"

char* return_hand_type(int type) {
    char* s = malloc(16);
    const char* rank_names[] = { "Unbekannt", "High Card", "Paar", "Trips", "Quads", "Straße", "Zwei-Paar Straße" };
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
        if (valid) return ZWEI_PAAR_STRASSE;
    }

    return INVALID;
}

// check if all selected cards are rank two
int is_hand_two(Card *hand, int size) {
    for (int i = 0; i < size; i++) {
        if (hand[i].rank != ZWEI) {
            return 0;
        }
    }
    return 1;
}

int is_valid_hand(Card *hand1, Card *hand2, int size1, int size2) {
    int type1 = get_hand_type(hand1, size1);
    int type2 = get_hand_type(hand2, size2);

    if (type1 == INVALID || type2 == INVALID) {
        return 0;
    }

    if (type1 == type2) {
        return 1;
    }

    int hand1_is_bomb = (type1 == QUADS) || (type1 == ZWEI_PAAR_STRASSE);
    int is_hand2_two = 0;
    int hand2_type = 0;

    if (type2 == HIGH && is_hand_two(hand2, size2)) {
        is_hand2_two = 1;
        hand2_type = HIGH;
    } else if (type2 == PAIR && is_hand_two(hand2, size2)) {
        is_hand2_two = 1;
        hand2_type = PAIR;
    } else if (type2 == TRIPS && is_hand_two(hand2, size2)) {
        is_hand2_two = 1;
        hand2_type = TRIPS;
    }

    if (hand1_is_bomb && is_hand2_two) {
        if (type1 == QUADS) {
            return hand2_type != TRIPS; // quads beat high card / pair of twos but not trips of two
        } else if (type1 == ZWEI_PAAR_STRASSE) {

            int num_pairs = size1 / 2;
            switch (hand2_type) {
                case HIGH: return num_pairs >= 3; // high card of two needs 3 pairs
                case PAIR: return num_pairs >= 4; // ...needs 4 pairs
                case TRIPS: return num_pairs >= 5; // ...needs 5 pairs
                default:
            }
        }
    }
    return 0;
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
    for (int i = 1; i < size; i++) {
        if (hand[i].suit > max) max = hand[i].suit;
    }
    return max;
}

Card get_highest_card(Card* hand, int size) {
    Card highest = hand[0];
    for (int i = 1; i < size; i++) {
        if (compare_cards(hand[i], highest)) highest = hand[i];
    }
    return highest;
}

int is_hand_higher(Card *hand1, Card *hand2, int size1, int size2) {
    Hand type1 = get_hand_type(hand1, size1);
    Hand type2 = get_hand_type(hand2, size2);

    if (type1 != type2) {
        int hand1_is_bomb = (type1 == QUADS) || (type1 == ZWEI_PAAR_STRASSE);
        int hand2_is_zwei = 0;

        if (type2 == HIGH && is_hand_two(hand2, size2) || type2 == PAIR && is_hand_two(hand2, size2) || type2 == TRIPS && is_hand_two(hand2, size2)) {
            hand2_is_zwei = 1;
        }

        if (hand1_is_bomb && hand2_is_zwei) {
            return 1; // bomb higher if valid
        }
        return 0;
    }

    if (type1 == HIGH) {
        if (hand1[0].rank == ZWEI && hand2[0].rank == ZWEI) {
            return compare_suits(hand1[0].suit, hand2[0].suit);
        } else if (hand1[0].rank == ZWEI) {
            return 1;
        } else if (hand2[0].rank == ZWEI) {
            return 0;
        } else {
            return compare_ranks(hand1[0].rank, hand2[0].rank);
        }
    } else if (type1 == PAIR || type1 == TRIPS) {
        if (hand1[0].rank != hand2[0].rank) {
            return compare_ranks(hand1[0].rank, hand2[0].rank);
        }

        return compare_suits(get_max_suit(hand1, size1), get_max_suit(hand2, size2));
    } else if (type1 == STRASSE) {
        Card highest1 = get_highest_card(hand1, size1);
        Card highest2 = get_highest_card(hand2, size2);

        return compare_cards(highest1, highest2);
    }
    return 0;
}


