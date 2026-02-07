#ifndef TYPES_H
#define TYPES_H

/*
 * C64 Chess Engine - Core Types and Constants
 *
 * Piece encoding (byte):
 *   Bits 0-2: piece type (1=Pawn, 2=Knight, 3=Bishop, 4=Rook, 5=Queen, 6=King)
 *   Bit 7:    color (0=White, 1=Black)
 *   0x00 = empty square
 *
 * 0x88 Board:
 *   128-byte array, only squares with (sq & 0x88) == 0 are valid
 *   Rank = sq >> 4, File = sq & 7
 *   Off-board detection: if (sq & 0x88) then invalid
 */

/* Use appropriate integer types */
#ifdef TARGET_C64
  /* cc65: use unsigned char/int for speed on 6502 */
  typedef unsigned char  u8;
  typedef signed char    s8;
  typedef unsigned int   u16;
  typedef signed int     s16;
  typedef unsigned long  u32;
  typedef signed long    s32;
#else
  #include <stdint.h>
  typedef uint8_t   u8;
  typedef int8_t    s8;
  typedef uint16_t  u16;
  typedef int16_t   s16;
  typedef uint32_t  u32;
  typedef int32_t   s32;
#endif

/* Boolean */
#define FALSE 0
#define TRUE  1

/* Colors */
#define WHITE 0
#define BLACK 1
#define COLOR_MASK 0x80

/* Piece types (bits 0-2) */
#define EMPTY  0
#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6
#define PIECE_MASK 0x07

/* Full piece values (color | type) */
#define W_PAWN   (PAWN)
#define W_KNIGHT (KNIGHT)
#define W_BISHOP (BISHOP)
#define W_ROOK   (ROOK)
#define W_QUEEN  (QUEEN)
#define W_KING   (KING)
#define B_PAWN   (COLOR_MASK | PAWN)
#define B_KNIGHT (COLOR_MASK | KNIGHT)
#define B_BISHOP (COLOR_MASK | BISHOP)
#define B_ROOK   (COLOR_MASK | ROOK)
#define B_QUEEN  (COLOR_MASK | QUEEN)
#define B_KING   (COLOR_MASK | KING)

/* Piece accessors */
#define PIECE_TYPE(p)   ((p) & PIECE_MASK)
#define PIECE_COLOR(p)  (((p) & COLOR_MASK) ? BLACK : WHITE)
#define IS_WHITE(p)     (!((p) & COLOR_MASK))
#define IS_BLACK(p)     ((p) & COLOR_MASK)
#define IS_SLIDER(p)    (PIECE_TYPE(p) >= BISHOP && PIECE_TYPE(p) <= QUEEN)
#define MAKE_PIECE(color, type) ((color) ? (COLOR_MASK | (type)) : (type))

/* 0x88 square macros */
#define SQ_VALID(sq)    (!((sq) & 0x88))
#define SQ_RANK(sq)     ((sq) >> 4)
#define SQ_FILE(sq)     ((sq) & 0x07)
#define SQ_MAKE(r, f)   (((r) << 4) | (f))
#define SQ_FLIP(sq)     ((sq) ^ 0x70)   /* mirror vertically for black PST */
#define SQ_INDEX64(sq)  ((SQ_RANK(sq) << 3) | SQ_FILE(sq))  /* 0x88 -> 0..63 */

/* Named squares */
#define SQ_A1 0x00
#define SQ_B1 0x01
#define SQ_C1 0x02
#define SQ_D1 0x03
#define SQ_E1 0x04
#define SQ_F1 0x05
#define SQ_G1 0x06
#define SQ_H1 0x07
#define SQ_A8 0x70
#define SQ_B8 0x71
#define SQ_C8 0x72
#define SQ_D8 0x73
#define SQ_E8 0x74
#define SQ_F8 0x75
#define SQ_G8 0x76
#define SQ_H8 0x77

#define SQ_NONE 0xFF

/* Direction offsets for 0x88 board */
#define DIR_N    16
#define DIR_S   (-16)
#define DIR_E    1
#define DIR_W   (-1)
#define DIR_NE   17
#define DIR_NW   15
#define DIR_SE  (-15)
#define DIR_SW  (-17)

/* Move flags (packed in 1 byte) */
#define MF_NONE     0x00
#define MF_CAPTURE  0x01
#define MF_CASTLE   0x02
#define MF_EP       0x04   /* en passant capture */
#define MF_PAWNSTART 0x08  /* double pawn push */
#define MF_PROMO    0x10   /* promotion (promo piece in bits 5-6) */

/* Promotion piece encoding in flags bits 5-6 */
#define MF_PROMO_N  (MF_PROMO | (0 << 5))
#define MF_PROMO_B  (MF_PROMO | (1 << 5))
#define MF_PROMO_R  (MF_PROMO | (2 << 5))
#define MF_PROMO_Q  (MF_PROMO | (3 << 5))
#define PROMO_TYPE(flags) (KNIGHT + (((flags) >> 5) & 3))

/* Move structure - 4 bytes, fits well on 6502 */
typedef struct {
    u8 from;      /* source square (0x88) */
    u8 to;        /* target square (0x88) */
    u8 flags;     /* move flags */
    u8 score;     /* move ordering score */
} Move;

/* Move is "null" if from == to == 0 */
#define MOVE_NONE_FROM 0
#define MOVE_NONE_TO   0
#define IS_MOVE_NONE(m) ((m).from == MOVE_NONE_FROM && (m).to == MOVE_NONE_TO)

/* Castling rights (bitmask) */
#define CASTLE_WK 0x01  /* White kingside */
#define CASTLE_WQ 0x02  /* White queenside */
#define CASTLE_BK 0x04  /* Black kingside */
#define CASTLE_BQ 0x08  /* Black queenside */
#define CASTLE_ALL 0x0F

/* Score constants */
#define SCORE_INFINITY 30000
#define SCORE_MATE     29000
#define SCORE_MATED   (-29000)
#define SCORE_DRAW     0

#define IS_MATE_SCORE(s) ((s) > SCORE_MATE - 100 || (s) < SCORE_MATED + 100)

/* Search limits */
#define MAX_PLY     64
#define MAX_MOVES   256   /* max moves per position (generous) */
#define MAX_GAME_MOVES 512

/* Undo information for unmake_move */
typedef struct {
    u8  captured;      /* captured piece (or EMPTY) */
    u8  castle_rights; /* castling rights before move */
    u8  ep_square;     /* en passant square before move */
    u8  fifty_clock;   /* fifty-move counter before move */
    u16 hash;          /* Zobrist hash before move */
    s16 material[2];   /* material scores before move */
    s16 pst_score[2];  /* piece-square scores before move */
} Undo;

/* Transposition table entry - 8 bytes */
#define TT_FLAG_EXACT  0
#define TT_FLAG_ALPHA  1   /* upper bound (fail-low) */
#define TT_FLAG_BETA   2   /* lower bound (fail-high) */

typedef struct {
    u16 key;       /* verification key (upper bits of Zobrist hash) */
    s16 score;     /* evaluation score */
    Move best;     /* best move (from, to, flags - score field unused) */
    u8  depth;     /* search depth (lower 6 bits) + flag (upper 2 bits) */
} TTEntry;

#define TT_SIZE 512     /* number of entries (4KB total) */

/* Flat move buffer - shared across all plies */
#define MOVE_BUF_SIZE 4096

/* Global game/board state - declared in board.c */
typedef struct {
    u8  board[128];       /* 0x88 board */
    u8  side;             /* side to move (WHITE/BLACK) */
    u8  castle_rights;    /* castling rights bitmask */
    u8  ep_square;        /* en passant target square (SQ_NONE if none) */
    u8  fifty_clock;      /* fifty-move rule counter */
    u16 ply;              /* half-move clock (total) */
    u16 hash;             /* 16-bit Zobrist hash */
    u8  king_sq[2];       /* king squares [WHITE/BLACK] */
    s16 material[2];      /* material score [WHITE/BLACK] */
    s16 pst_score[2];     /* piece-square table score [WHITE/BLACK] */

    /* Undo stack */
    Undo undo_stack[MAX_GAME_MOVES];
    u16  undo_ply;

    /* Move buffer - flat array shared by all plies */
    Move move_buf[MOVE_BUF_SIZE];
    u16  move_buf_idx[MAX_PLY + 1]; /* start index for each ply */

    /* History for repetition detection */
    u16 hash_history[MAX_GAME_MOVES];
    u16 hash_hist_count;
} GameState;

extern GameState g_state;

#endif /* TYPES_H */
