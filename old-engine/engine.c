#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/time.h>

// Minimal UCI engine: first legal move -> fully fledged engine ported from Python
// No castling, no en-passant; promotions -> queen only.

typedef struct {
    int from, to;
    char promo;
} Move;

typedef struct {
    char b[64];
    int white_to_move;
} Pos;

// --- Global History & TT Data ---
Pos game_history[1024];
int game_hist_len = 0;

typedef struct {
    uint64_t key;
    int depth;
    int value;
    int type; // 1=EXACT, 2=LOWER, 3=UPPER
} TTEntry;

#define TT_SIZE 1048576
TTEntry *tt = NULL;

double search_start_time = 0.0;
double search_time_limit = 0.0;
int timeout_flag = 0;

FILE *engine_log = NULL;

// --- Utilities ---

double get_time_s() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

void check_time() {
    if (get_time_s() - search_start_time > search_time_limit) {
        timeout_flag = 1;
    }
}

void log_msg(const char *msg) {
    if (!engine_log) engine_log = fopen("engine_debug.log", "w");
    if (engine_log) {
        fprintf(engine_log, "%s\n", msg);
        fflush(engine_log);
    }
}

void log_depth(const char *msg) {
    FILE *f = fopen("depth_debug_c.txt", "a");
    if (f) {
        fprintf(f, "%s", msg);
        fclose(f);
    }
}

void clear_depth_log() {
    FILE *f = fopen("depth_debug_c.txt", "w");
    if (f) {
        fclose(f);
    }
}

static int sq_index(const char *s) {
    int file = s[0] - 'a';
    int rank = s[1] - '1';
    return rank * 8 + file;
}

static void index_to_sq(int idx, char out[3]) {
    out[0] = (char) ('a' + (idx % 8));
    out[1] = (char) ('1' + (idx / 8));
    out[2] = 0;
}

void move_to_uci(Move m, char *out) {
    if (m.from == 0 && m.to == 0 && m.promo == 0) {
        strcpy(out, "0000");
        return;
    }
    index_to_sq(m.from, out);
    index_to_sq(m.to, out + 2);
    if (m.promo) {
        out[4] = m.promo;
        out[5] = 0;
    } else {
        out[4] = 0;
    }
}

// --- Position Parsing ---

static void pos_from_fen(Pos *p, const char *fen) {
    memset(p->b, '.', 64);
    p->white_to_move = 1;

    char buf[256];
    strncpy(buf, fen, sizeof(buf)-1);
    buf[sizeof(buf) - 1] = 0;

    char *save = NULL;
    char *placement = strtok_r(buf, " ", &save);
    char *stm = strtok_r(NULL, " ", &save);
    if (stm) p->white_to_move = (strcmp(stm, "w") == 0);

    int rank = 7, file = 0;
    for (size_t i = 0; placement && placement[i]; i++) {
        char c = placement[i];
        if (c == '/') {
            rank--;
            file = 0;
            continue;
        }
        if (isdigit((unsigned char) c)) {
            file += c - '0';
            continue;
        }
        int idx = rank * 8 + file;
        if (idx >= 0 && idx < 64) p->b[idx] = c;
        file++;
    }
}

static void pos_start(Pos *p) {
    pos_from_fen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1");
}

static int is_white_piece(char c) { return c >= 'A' && c <= 'Z'; }

// --- Check & Attack Detection ---

static int is_square_attacked(const Pos *p, int sq, int by_white) {
    int r = sq / 8, f = sq % 8;

    // pawns
    if (by_white) {
        if (r > 0 && f > 0 && p->b[(r - 1) * 8 + (f - 1)] == 'P') return 1;
        if (r > 0 && f < 7 && p->b[(r - 1) * 8 + (f + 1)] == 'P') return 1;
    } else {
        if (r < 7 && f > 0 && p->b[(r + 1) * 8 + (f - 1)] == 'p') return 1;
        if (r < 7 && f < 7 && p->b[(r + 1) * 8 + (f + 1)] == 'p') return 1;
    }

    // knights
    static const int nd[8] = {-17, -15, -10, -6, 6, 10, 15, 17};
    for (int i = 0; i < 8; i++) {
        int to = sq + nd[i];
        if (to < 0 || to >= 64) continue;
        int tr = to / 8, tf = to % 8;
        int dr = tr - r; if (dr < 0) dr = -dr;
        int df = tf - f; if (df < 0) df = -df;
        if (!((dr == 1 && df == 2) || (dr == 2 && df == 1))) continue;
        char pc = p->b[to];
        if (by_white && pc == 'N') return 1;
        if (!by_white && pc == 'n') return 1;
    }

    // sliders
    static const int dirs[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (int di = 0; di < 8; di++) {
        int df = dirs[di][0], dr = dirs[di][1];
        int cr = r + dr, cf = f + df;
        while (cr >= 0 && cr < 8 && cf >= 0 && cf < 8) {
            int idx = cr * 8 + cf;
            char pc = p->b[idx];
            if (pc != '.') {
                int pc_white = is_white_piece(pc);
                if (pc_white == by_white) {
                    char up = (char) toupper((unsigned char) pc);
                    int rook_dir = (di < 4);
                    int bishop_dir = (di >= 4);
                    if (up == 'Q') return 1;
                    if (rook_dir && up == 'R') return 1;
                    if (bishop_dir && up == 'B') return 1;
                    if (up == 'K' && (abs(cr - r) <= 1 && abs(cf - f) <= 1)) return 1;
                }
                break;
            }
            cr += dr;
            cf += df;
        }
    }

    // king adjacency
    for (int rr = r - 1; rr <= r + 1; rr++) {
        for (int ff = f - 1; ff <= f + 1; ff++) {
            if (rr < 0 || rr >= 8 || ff < 0 || ff >= 8) continue;
            if (rr == r && ff == f) continue;
            char pc = p->b[rr * 8 + ff];
            if (by_white && pc == 'K') return 1;
            if (!by_white && pc == 'k') return 1;
        }
    }
    return 0;
}

static int in_check(const Pos *p, int white_king) {
    char k = white_king ? 'K' : 'k';
    int ksq = -1;
    for (int i = 0; i < 64; i++) {
        if (p->b[i] == k) {
            ksq = i;
            break;
        }
    }
    if (ksq < 0) return 1;
    return is_square_attacked(p, ksq, !white_king);
}

static Pos make_move(const Pos *p, Move m) {
    Pos np = *p;
    char piece = np.b[m.from];
    np.b[m.from] = '.';
    char placed = piece;
    if (m.promo && (piece == 'P' || piece == 'p')) {
        placed = is_white_piece(piece)
                     ? (char) toupper((unsigned char) m.promo)
                     : (char) tolower((unsigned char) m.promo);
    }
    np.b[m.to] = placed;
    np.white_to_move = !p->white_to_move;
    return np;
}

static void add_move(Move *moves, int *n, int from, int to, char promo) {
    moves[*n].from = from;
    moves[*n].to = to;
    moves[*n].promo = promo;
    (*n)++;
}

// --- Move Generation ---

static void gen_pawn(const Pos *p, int from, int white, Move *moves, int *n) {
    int r = from / 8, f = from % 8;
    int dir = white ? 1 : -1;
    int start_r = white ? 1 : 6;
    int prom_r = white ? 7 : 0;

    int nr = r + dir;
    if (nr >= 0 && nr < 8) {
        int to = nr * 8 + f;
        if (p->b[to] == '.') {
            char promo = (nr == prom_r) ? 'q' : 0;
            add_move(moves, n, from, to, promo);
            if (r == start_r) {
                int to2 = (r + 2 * dir) * 8 + f;
                if (p->b[to2] == '.') {
                    add_move(moves, n, from, to2, 0);
                }
            }
        }
        for (int df = -1; df <= 1; df += 2) {
            int nf = f + df;
            if (nf >= 0 && nf < 8) {
                int to_cap = nr * 8 + nf;
                char target = p->b[to_cap];
                if (target != '.' && is_white_piece(target) != white) {
                    char promo = (nr == prom_r) ? 'q' : 0;
                    add_move(moves, n, from, to_cap, promo);
                }
            }
        }
    }
}

static void gen_knight(const Pos *p, int from, int white, Move *moves, int *n) {
    static const int nd[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
    int r = from / 8, f = from % 8;
    for (int i=0; i<8; i++) {
        int nr = r + nd[i][0], nf = f + nd[i][1];
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int to = nr * 8 + nf;
            char target = p->b[to];
            if (target == '.' || is_white_piece(target) != white) {
                add_move(moves, n, from, to, 0);
            }
        }
    }
}

static void gen_sliding(const Pos *p, int from, int white, const int dirs[][2], int dcount, Move *moves, int *n) {
    int r = from / 8, f = from % 8;
    for (int di=0; di<dcount; di++) {
        int dr = dirs[di][0], df = dirs[di][1];
        int nr = r + dr, nf = f + df;
        while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
            int to = nr * 8 + nf;
            char target = p->b[to];
            if (target == '.') {
                add_move(moves, n, from, to, 0);
            } else {
                if (is_white_piece(target) != white) {
                    add_move(moves, n, from, to, 0);
                }
                break;
            }
            nr += dr; nf += df;
        }
    }
}

static void gen_queen(const Pos *p, int from, int white, const int dirs[][2], int dcount, Move *moves, int *n) { gen_sliding(p, from, white, dirs, dcount, moves, n); }
static void gen_bishop(const Pos *p, int from, int white, const int dirs[][2], int dcount, Move *moves, int *n) { gen_sliding(p, from, white, dirs, dcount, moves, n); }
static void gen_rook(const Pos *p, int from, int white, const int dirs[][2], int dcount, Move *moves, int *n) { gen_sliding(p, from, white, dirs, dcount, moves, n); }

static void gen_king(const Pos *p, int from, int white, Move *moves, int *n) {
    int r = from / 8, f = from % 8;
    for (int dr = -1; dr <= 1; dr++) {
        for (int df = -1; df <= 1; df++) {
            if (dr == 0 && df == 0) continue;
            int nr = r + dr, nf = f + df;
            if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
                int to = nr * 8 + nf;
                char target = p->b[to];
                if (target == '.' || is_white_piece(target) != white) {
                    add_move(moves, n, from, to, 0);
                }
            }
        }
    }
}

static int pseudo_legal_moves(const Pos *p, Move *moves) {
    int n = 0;
    int us_white = p->white_to_move;
    for (int i = 0; i < 64; i++) {
        char pc = p->b[i];
        if (pc == '.') continue;
        int white = is_white_piece(pc);
        if (white != us_white) continue;
        char up = (char) toupper((unsigned char) pc);
        
        if (up == 'P') gen_pawn(p, i, white, moves, &n);
        else if (up == 'N') gen_knight(p, i, white, moves, &n);
        else if (up == 'B') {
            static const int d[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            gen_bishop(p, i, white, d, 4, moves, &n);
        } else if (up == 'R') {
            static const int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            gen_rook(p, i, white, d, 4, moves, &n);
        } else if (up == 'Q') {
            static const int d[8][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            gen_queen(p, i, white, d, 8, moves, &n);
        } else if (up == 'K') gen_king(p, i, white, moves, &n);
    }
    return n;
}

static int legal_moves(const Pos *p, Move *out) {
    Move tmp[256];
    int pn = pseudo_legal_moves(p, tmp);
    int n = 0;
    for (int i = 0; i < pn; i++) {
        Pos np = make_move(p, tmp[i]);
        if (!in_check(&np, !np.white_to_move)) {
            out[n++] = tmp[i];
        }
    }
    return n;
}

static void apply_uci_move(Pos *p, const char *uci) {
    if (!uci || strlen(uci) < 4) return;
    Move m;
    m.from = sq_index(uci);
    m.to = sq_index(uci + 2);
    m.promo = (strlen(uci) >= 5) ? uci[4] : 0;
    Pos np = make_move(p, m);
    *p = np;
}

static void parse_position(Pos *p, const char *line) {
    char buf[1024];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf) - 1] = 0;

    game_hist_len = 0; // Reset history

    char *toks[128];
    int nt = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " \t\r\n", &save); tok && nt < 128; tok = strtok_r(NULL, " \t\r\n", &save)) {
        toks[nt++] = tok;
    }

    int i = 1;
    if (i < nt && strcmp(toks[i], "startpos") == 0) {
        pos_start(p);
        game_history[game_hist_len++] = *p;
        i++;
    } else if (i < nt && strcmp(toks[i], "fen") == 0) {
        i++;
        char fen[512] = {0};
        for (int k = 0; k < 6 && i < nt; k++, i++) {
            if (k) strcat(fen, " ");
            strcat(fen, toks[i]);
        }
        pos_from_fen(p, fen);
        game_history[game_hist_len++] = *p;
    }

    if (i < nt && strcmp(toks[i], "moves") == 0) {
        i++;
        for (; i < nt; i++) {
            apply_uci_move(p, toks[i]);
            game_history[game_hist_len++] = *p;
        }
    }
}

static void print_bestmove(Move m) {
    char uci[6];
    move_to_uci(m, uci);
    printf("bestmove %s\n", uci);
    fflush(stdout);
}

// --- AI Evaluation & Search ---

// --- AI Evaluation & Search ---

int evaluate(const Pos *p) {
    // Base piece values (Centipawns)
    const int val_P = 100, val_N = 320, val_B = 330, val_R = 500, val_Q = 900, val_K = 20000;

    // PeSTO Tables
    static const int pst_P[64] = {
          0,   0,   0,   0,   0,   0,   0,   0,
         78,  83,  86,  73, 102,  82,  85,  90,
          7,  29,  21,  44,  40,  31,  44,   7,
        -17,  16,  -2,  15,  14,   0,  15, -13,
        -26,   3,  10,   9,   6,   1,   0, -23,
        -22,   9,   5, -11, -10,  -2,   3, -19,
        -31,   8,  -7, -37, -36, -14,   3, -31,
          0,   0,   0,   0,   0,   0,   0,   0
    };
    static const int pst_N[64] = {
        -66, -53, -75, -75, -10, -33, -58, -66,
         -3,  -6, 100, -36,   4,  62,  -4, -14,
         10,  67,   1,  74,  73,  27,  62,  -2,
         24,  24,  45,  37,  33,  41,  25,  17,
         -1,   5,  31,  21,  22,  35,   2,   0,
        -18,  10,  13,  22,  18,  15,  11, -14,
        -23, -15,   2,   0,   2,   0, -23, -20,
        -74, -23, -26, -24, -19, -35, -22, -69
    };
    static const int pst_B[64] = {
        -59, -78, -82, -76, -23,-107, -37, -50,
        -11,  20,  35, -42, -39,  31,   2, -22,
         -9,  39, -32,  41,  52, -10,  28, -14,
         25,  17,  20,  34,  26,  25,  15,  10,
         13,  10,  17,  23,  17,  16,   0,   7,
         14,  25,  24,  15,   8,  25,  20,  15,
         19,  20,  11,   6,   7,   6,  20,  16,
         -7,   2, -15, -12, -14, -15, -10, -10
    };
    static const int pst_R[64] = {
         35,  29,  33,   4,  37,  33,  56,  50,
         55,  29,  56,  67,  55,  62,  34,  60,
         19,  35,  28,  33,  45,  27,  25,  15,
          0,   5,  16,  13,  18,  -4,  -9,  -6,
        -28, -35, -16, -21, -13, -29, -46, -30,
        -42, -28, -42, -25, -25, -35, -26, -46,
        -53, -38, -31, -26, -29, -43, -44, -53,
        -30, -24, -18,   5,  -2, -18, -31, -32
    };
    static const int pst_Q[64] = {
          6,   1,  -8,-104,  69,  24,  88,  26,
         14,  32,  60, -10,  20,  76,  57,  24,
         -2,  43,  32,  60,  72,  63,  43,   2,
          1, -16,  22,  17,  25,  20, -13,  -6,
        -14, -15,  -2,  -5,  -1, -10, -20, -22,
        -30,  -6, -13, -11, -16, -11, -16, -27,
        -36, -18,   0, -19, -15, -15, -21, -38,
        -39, -30, -31, -13, -31, -36, -34, -42
    };
    static const int pst_K[64] = {
          4,  54,  47, -99, -99,  60,  83, -62,
        -32,  10,  55,  56,  56,  55,  10,   3,
        -62,  12, -57,  44, -67,  28,  37, -31,
        -55,  50,  11,  -4, -19,  13,   0, -49,
        -55, -43, -52, -28, -51, -47,  -8, -50,
        -47, -42, -43, -79, -64, -32, -29, -32,
          4,   3, -14, -50, -57, -18,  13,   4,
         17,  30,  -3, -14,   6,  -1,  40,  18
    };

    int score = 0;
    for (int i = 0; i < 64; i++) {
        char piece = p->b[i];
        if (piece == '.') continue;
        
        int is_w = is_white_piece(piece);
        char up = (char)toupper((unsigned char)piece);
        
        // Find the correct index based on player side (Black's view is mirrored)
        int r = i / 8;
        int f = i % 8;
        int pesto_idx = is_w ? ((7 - r) * 8 + f) : (r * 8 + f);
        
        int val = 0;
        if (up == 'P') val = val_P + pst_P[pesto_idx];
        else if (up == 'N') val = val_N + pst_N[pesto_idx];
        else if (up == 'B') val = val_B + pst_B[pesto_idx];
        else if (up == 'R') val = val_R + pst_R[pesto_idx];
        else if (up == 'Q') val = val_Q + pst_Q[pesto_idx];
        else if (up == 'K') val = val_K + pst_K[pesto_idx];

        score += is_w ? val : -val;
    }
    return score;
}

int move_score(Move m, const Pos *current_board) {
    char target = current_board->b[m.to];
    if (target == '.') return 0;
    char attacker = current_board->b[m.from];
    int t_val = 0, a_val = 0;
    char tu = toupper(target), au = toupper(attacker);
    if(tu=='P') t_val=1; else if(tu=='N'||tu=='B') t_val=3; else if(tu=='R') t_val=5; else if(tu=='Q') t_val=9; else if(tu=='K') t_val=100;
    if(au=='P') a_val=1; else if(au=='N'||au=='B') a_val=3; else if(au=='R') a_val=5; else if(au=='Q') a_val=9; else if(au=='K') a_val=100;
    return 1000 + (t_val * 10) - a_val;
}

void sort_moves(Move *moves, int n, const Pos *board) {
    int scores[256];
    for (int i=0; i<n; i++) scores[i] = move_score(moves[i], board);
    for (int i=1; i<n; i++) {
        Move tmp_m = moves[i];
        int tmp_s = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < tmp_s) {
            scores[j+1] = scores[j];
            moves[j+1] = moves[j];
            j--;
        }
        scores[j+1] = tmp_s;
        moves[j+1] = tmp_m;
    }
}

uint64_t get_hash(const Pos *p) {
    uint64_t h = 14695981039346656037ULL;
    for (int i = 0; i < 64; i++) {
        h ^= (uint8_t)p->b[i];
        h *= 1099511628211ULL;
    }
    h ^= p->white_to_move;
    h *= 1099511628211ULL;
    return h;
}

int quiescence(Pos *board, int alpha, int beta, int maximizing_player) {
    check_time();
    if (timeout_flag) return 0;

    int stand_pat = evaluate(board);
    if (maximizing_player) {
        if (stand_pat >= beta) return beta;
        if (stand_pat > alpha) alpha = stand_pat;
    } else {
        if (stand_pat <= alpha) return alpha;
        if (stand_pat < beta) beta = stand_pat;
    }

    Move moves[256], caps[256];
    int num_moves = legal_moves(board, moves), num_caps = 0;
    for (int i=0; i<num_moves; i++) {
        if (board->b[moves[i].to] != '.') caps[num_caps++] = moves[i];
    }
    sort_moves(caps, num_caps, board);

    if (maximizing_player) {
        int max_eval = stand_pat;
        for (int i=0; i<num_caps; i++) {
            Pos next_b = make_move(board, caps[i]);
            int eval_score = quiescence(&next_b, alpha, beta, 0);
            if (eval_score > max_eval) max_eval = eval_score;
            if (alpha < eval_score) alpha = eval_score;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = stand_pat;
        for (int i=0; i<num_caps; i++) {
            Pos next_b = make_move(board, caps[i]);
            int eval_score = quiescence(&next_b, alpha, beta, 1);
            if (eval_score < min_eval) min_eval = eval_score;
            if (beta > eval_score) beta = eval_score;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

int alpha_beta(Pos *board, int depth, int alpha, int beta, int maximizing_player, Pos *hist, int hist_len) {
    check_time();
    if (timeout_flag) return 0;

    // Repetition check
    int reps = 0;
    for (int i = 0; i < hist_len; i++) {
        if (memcmp(hist[i].b, board->b, 64) == 0) reps++;
    }
    if (reps >= 2) return 0;

    uint64_t hk = get_hash(board);
    int tt_idx = hk % TT_SIZE;
    if (tt[tt_idx].key == hk && tt[tt_idx].depth >= depth) {
        if (tt[tt_idx].type == 1) return tt[tt_idx].value; // EXACT
        if (tt[tt_idx].type == 2 && tt[tt_idx].value >= beta) return tt[tt_idx].value; // LOWER
        if (tt[tt_idx].type == 3 && tt[tt_idx].value <= alpha) return tt[tt_idx].value; // UPPER
    }

    if (depth == 0) return quiescence(board, alpha, beta, maximizing_player);

    Move moves[256];
    int num_moves = legal_moves(board, moves);
    if (num_moves == 0) return maximizing_player ? -99999 : 99999;

    sort_moves(moves, num_moves, board);
    int orig_alpha = alpha, orig_beta = beta;

    if (maximizing_player) {
        int max_eval = -999999;
        for (int i=0; i<num_moves; i++) {
            Pos next_b = make_move(board, moves[i]);
            hist[hist_len] = next_b;
            int eval_score = alpha_beta(&next_b, depth - 1, alpha, beta, 0, hist, hist_len + 1);
            if (eval_score > max_eval) max_eval = eval_score;
            if (alpha < eval_score) alpha = eval_score;
            if (beta <= alpha) break;
        }
        int flag = 1; // EXACT
        if (max_eval <= orig_alpha) flag = 3; // UPPER
        else if (max_eval >= beta) flag = 2; // LOWER
        tt[tt_idx].key = hk; tt[tt_idx].depth = depth; tt[tt_idx].value = max_eval; tt[tt_idx].type = flag;
        return max_eval;
    } else {
        int min_eval = 999999;
        for (int i=0; i<num_moves; i++) {
            Pos next_b = make_move(board, moves[i]);
            hist[hist_len] = next_b;
            int eval_score = alpha_beta(&next_b, depth - 1, alpha, beta, 1, hist, hist_len + 1);
            if (eval_score < min_eval) min_eval = eval_score;
            if (beta > eval_score) beta = eval_score;
            if (beta <= alpha) break;
        }
        int flag = 1;
        if (min_eval >= orig_beta) flag = 2; // LOWER
        else if (min_eval <= alpha) flag = 3; // UPPER
        tt[tt_idx].key = hk; tt[tt_idx].depth = depth; tt[tt_idx].value = min_eval; tt[tt_idx].type = flag;
        return min_eval;
    }
}

Move find_best_move(Pos *board, double t_limit) {
    search_start_time = get_time_s();
    search_time_limit = t_limit;
    timeout_flag = 0;

    Move root_moves[256];
    int num_root = legal_moves(board, root_moves);
    if (num_root == 0) {
        Move empty = {0};
        return empty;
    }

    Move best_overall = root_moves[0];
    char buf[512];
    sprintf(buf, "\n--- NEW TURN: Thinking for %.2fs ---\n", search_time_limit);
    log_depth(buf);

    memset(tt, 0, sizeof(TTEntry) * TT_SIZE);

    Pos hist[2048]; // local search history
    for (int i=0; i<game_hist_len; i++) hist[i] = game_history[i];

    for (int current_depth = 1; current_depth < 100; current_depth++) {
        if (timeout_flag) break;
        sort_moves(root_moves, num_root, board);
        Move best_this_depth = root_moves[0];

        int best_value = board->white_to_move ? -999999 : 999999;

        for (int i=0; i<num_root; i++) {
            Pos next_b = make_move(board, root_moves[i]);
            hist[game_hist_len] = next_b;
            int val = alpha_beta(&next_b, current_depth - 1, -999999, 999999, !board->white_to_move, hist, game_hist_len + 1);
            if (timeout_flag) break;

            if (board->white_to_move) {
                if (val > best_value) { best_value = val; best_this_depth = root_moves[i]; }
            } else {
                if (val < best_value) { best_value = val; best_this_depth = root_moves[i]; }
            }
        }
        if (timeout_flag) break;
        best_overall = best_this_depth;

        char uci[6];
        move_to_uci(best_this_depth, uci);
        printf("info depth %d currmove %s\n", current_depth, uci);
        fflush(stdout);

        sprintf(buf, "Reached Depth %d | Best Move: %s\n", current_depth, uci);
        log_depth(buf);

        if (best_value >= 90000 || best_value <= -90000) {
            log_depth("Forced Checkmate found! Terminating search early.\n");
            break;
        }
    }

    if (timeout_flag) {
        char uci[6];
        move_to_uci(best_overall, uci);
        sprintf(buf, "TIMEOUT! Stopping. Playing: %s\n", uci);
        log_depth(buf);
    }
    return best_overall;
}

// --- Main Engine Loop ---

int main(void) {
    tt = calloc(TT_SIZE, sizeof(TTEntry));
    Pos pos;
    pos_start(&pos);
    game_history[game_hist_len++] = pos;

    log_msg("Engine successfully launched!");
    clear_depth_log();

    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;
        if (!len) continue;

        char logbuf[1200];
        sprintf(logbuf, "Received from Java: %s", line);
        log_msg(logbuf);

        if (strcmp(line, "uci") == 0) {
            printf("id name c_engine\n");
            printf("id author c_dev\n");
            printf("uciok\n");
            fflush(stdout);
            log_msg("Sent uciok");
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
            log_msg("Sent readyok");
        } else if (strcmp(line, "ucinewgame") == 0) {
            pos_start(&pos);
            game_hist_len = 0;
            game_history[game_hist_len++] = pos;
            clear_depth_log();
            log_msg("Board reset");
        } else if (strncmp(line, "position", 8) == 0) {
            parse_position(&pos, line);
            log_msg("Position parsed");
        } else if (strncmp(line, "go", 2) == 0) {
            log_msg("Thinking...");
            
            double allocated_time = 9.5;
            char *tok = strstr(line, "movetime");
            if (tok) {
                int ms;
                if (sscanf(tok + 9, "%d", &ms) == 1) {
                    allocated_time = (ms / 1000.0) - 0.05;
                    if (allocated_time < 0.1) allocated_time = 0.1;
                }
            } else {
                char time_str[16] = "";
                if (pos.white_to_move) strcpy(time_str, "wtime");
                else strcpy(time_str, "btime");

                tok = strstr(line, time_str);
                if (tok) {
                    int time_left_ms;
                    if (sscanf(tok + strlen(time_str) + 1, "%d", &time_left_ms) == 1) {
                        allocated_time = (time_left_ms / 30000.0) - 0.05;
                        if (allocated_time < 0.1) allocated_time = 0.1;
                    }
                }
            }
            
            sprintf(logbuf, "Allocated %.2f seconds for this move.", allocated_time);
            log_msg(logbuf);

            Move m = find_best_move(&pos, allocated_time);
            print_bestmove(m);

            char uci[6];
            move_to_uci(m, uci);
            sprintf(logbuf, "Sent bestmove %s", uci);
            log_msg(logbuf);

        } else if (strcmp(line, "quit") == 0) {
            log_msg("Quit command received.");
            break;
        }
    }

    if (engine_log) fclose(engine_log);
    free(tt);
    return 0;
}