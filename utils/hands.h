//
// Created by must9 on 21.05.2025.
//

#ifndef TIENLEN_HANDS_H
#define TIENLEN_HANDS_H

typedef enum {
    INVALID, HIGH, PAIR, TRIPS, QUADS, STRASSE, ZWEIER_STRASSE, BOMB, WIN
} Hand;

int is_valid_hand(Card *hand1, Card *hand2, int size);
int get_hand_type(Card *hand, int size);
char* return_hand_type(int type);
int is_hand_higher(Card* hand1, Card* hand2, int size);

#endif //TIENLEN_HANDS_H
