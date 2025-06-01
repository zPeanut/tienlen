//
// Created by must9 on 01.06.2025.
//

#include <stdio.h>
#include <malloc.h>

char* int_to_str(int src) {
    char* buf = malloc( 10); // this should usually never be over 10 unless you are converting a really large number into a str
    snprintf(buf, sizeof(buf), "%i", src);
    return buf;
}
