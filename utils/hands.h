//
// Created by must9 on 21.05.2025.
//

#ifndef TIENLEN_HANDS_H
#define TIENLEN_HANDS_H

typedef enum {
    INVALID, HIGH, PAIR, TRIPS, QUADS, STRASSE, ZWEI_PAAR_STRASSE
} Hand;

int compare_hands(Card *hand1, Card *hand2, int size1, int size2);
int get_hand_type(Card *hand, int size);
char* return_hand_type(int type);
int is_hand_higher(Card *hand1, Card *hand2, int size1, int size2);

#endif //TIENLEN_HANDS_H
