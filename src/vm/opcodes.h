#ifndef AHHHHH_OPCODES_H
#define AHHHHH_OPCODES_H

#include <stdint.h>

// alle befehel die die vm besteht
typedef enum {
    OP_CONSTANT,      // lädt eine konstante aus dem chunk auf den stack
    OP_NULL,          // null auf den stack legen
    OP_TRUE,          // true auf den stack legen
    OP_FALSE,         // false auf den stack legen

    OP_LOAD_GLOBAL,   // lädt eine globale variable auf den stack
    OP_STORE_GLOBAL,  // speichert wert in einer globalen variable
    OP_LOAD_LOCAL,    // lädt eine lokale variable auf den stack
    OP_STORE_LOCAL,   // speichert wert in einer lokalen variable

    OP_ADD,           // zwei werte addieren
    OP_SUB,           // zwei werte subtrahieren
    OP_MUL,           // zwei werte multiplizieren
    OP_DIV,           // zwei werte dividieren
    OP_MOD,           // modulo berechnung

    OP_EQ,            // vergleicht ob werte gleich sind
    OP_NE,            // vergleicht ob werte ungleich sind
    OP_LT,            // vergleicht ob kleiner als
    OP_GT,            // vergleicht ob größer als
    OP_LE,            // vergleicht ob kleiner gleich
    OP_GE,            // vergleicht ob größer gleich

    OP_NOT,           // logisches nicht
    OP_NEG,           // zahl negieren (vorzeichen umkehren)

    OP_PRINT,         // wert auf der konsole ausgeben

    OP_JUMP,          // unbedingter sprung nach vorne
    OP_JUMP_IF_FALSE, // springt wenn der wert auf dem stack false ist

    OP_LOOP,          // schleifen sprung zurück

    OP_CALL,          // funktion aufrufen
    OP_RETURN,        // aus funktion zurückspringen

    OP_CLOSURE,       // eine closure (funktion mit umgebung) erstellen
    OP_CLOSE_UPVALUE, // upvalue schließen (auf den heap verschieben)

    OP_GET_GLOBAL,    // globalen wert holen
    OP_DEFINE_GLOBAL, // globale variable definieren

    OP_INDEX_GET,     // element an index holen (z.B. für arrays)
    OP_INDEX_SET,     // element an index setzen

    OP_GET_FIELD,     // feld aus einem struct/objekt holen
    OP_SET_FIELD,     // feld in einem struct/objekt setzen

    OP_ARRAY,         // neues array erstellen

    OP_ARRAY_GET,     // array element lesen
    OP_ARRAY_SET,     // array element schreiben
    OP_ARRAY_LEN,     // array länge abfragen

    OP_STRUCT,        // neues struct erstellen
    OP_TYPE,          // typen definition registrieren

    OP_POP,           // obersten wert vom stack werfen
    OP_DUP,           // obersten wert auf dem stack duplizieren

    OP_BREAK,         // schleife abbrechen
    OP_CONTINUE,      // nächster schleifendurchlauf

    OP_COUNT          // anzahl aller opcodes (für internen gebrauch)
} Opcode;

typedef enum {
    VALUE_NUMBER,    
    VALUE_STRING,    
    VALUE_BOOL,      
    VALUE_NULL,       
    VALUE_FUNCTION,   
    VALUE_NATIVE,     
    VALUE_OBJECT,     
    VALUE_ARRAY,    
} ValueKind;

#endif