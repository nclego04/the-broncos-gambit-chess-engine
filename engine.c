#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/time.h>

#define FLIP(sq) ((sq) ^ 56)

// Base Material Values: {Pawn, Knight, Bishop, Rook, Queen, King}
const int piece_value[6] = { 100, 320, 330, 500, 900, 20000 };

const int pawn_pst[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int knight_pst[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};

const int bishop_pst[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};

const int rook_pst[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

const int queen_pst[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};

const int king_pst[64] = {
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

// Array of pointers to cleanly loop through the tables
const int *pst_tables[6] = { pawn_pst, knight_pst, bishop_pst, rook_pst, queen_pst, king_pst };

typedef struct {
    int from, to;
    char promo;
} Move;

typedef struct {
    char b[64];
    int white_to_move;
} Pos;

double search_start_time = 0.0;
double search_time_limit = 0.0;
int timeout_flag = 0;
uint64_t nodes_searched = 0;

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
    if (f) fclose(f);
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

    if (by_white) {
        if (r > 0 && f > 0 && p->b[(r - 1) * 8 + (f - 1)] == 'P') return 1;
        if (r > 0 && f < 7 && p->b[(r - 1) * 8 + (f + 1)] == 'P') return 1;
    } else {
        if (r < 7 && f > 0 && p->b[(r + 1) * 8 + (f - 1)] == 'p') return 1;
        if (r < 7 && f < 7 && p->b[(r + 1) * 8 + (f + 1)] == 'p') return 1;
    }

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
        placed = is_white_piece(piece) ? (char)toupper((unsigned char)m.promo) : (char)tolower((unsigned char)m.promo);
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

    char *toks[128];
    int nt = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, " \t\r\n", &save); tok && nt < 128; tok = strtok_r(NULL, " \t\r\n", &save)) {
        toks[nt++] = tok;
    }

    int i = 1;
    if (i < nt && strcmp(toks[i], "startpos") == 0) {
        pos_start(p);
        i++;
    } else if (i < nt && strcmp(toks[i], "fen") == 0) {
        i++;
        char fen[512] = {0};
        for (int k = 0; k < 6 && i < nt; k++, i++) {
            if (k) strcat(fen, " ");
            strcat(fen, toks[i]);
        }
        pos_from_fen(p, fen);
    }

    if (i < nt && strcmp(toks[i], "moves") == 0) {
        i++;
        for (; i < nt; i++) {
            apply_uci_move(p, toks[i]);
        }
    }
}

static void print_bestmove(Move m) {
    char uci[6];
    move_to_uci(m, uci);
    printf("bestmove %s\n", uci);
    fflush(stdout);
}

// --- AI Evaluation & Search (BASELINE) ---

// --- BASIC PST EVALUATION ---
int evaluate(const Pos *board) {
    int white_score = 0;
    int black_score = 0;

    for (int sq = 0; sq < 64; sq++) {
        char pc = board->b[sq];
        if (pc == '.') continue;

        int is_white = isupper((unsigned char)pc);
        char upper_pc = (char)toupper((unsigned char)pc);
        int p_type = -1;

        // Map characters to the array indices
        if (upper_pc == 'P') p_type = 0;
        else if (upper_pc == 'N') p_type = 1;
        else if (upper_pc == 'B') p_type = 2;
        else if (upper_pc == 'R') p_type = 3;
        else if (upper_pc == 'Q') p_type = 4;
        else if (upper_pc == 'K') p_type = 5;

        if (p_type == -1) continue;

        // Flip the board index if evaluating White
        int table_sq = is_white ? FLIP(sq) : sq;

        // Add Base Material + Square Bonus
        int value = piece_value[p_type] + pst_tables[p_type][table_sq];

        if (is_white) {
            white_score += value;
        } else {
            black_score += value;
        }
    }

    // Absolute score (Positive = White winning, Negative = Black winning)
    return white_score - black_score;
}

// --- MOVE SORTING (MVV-LVA) ---
int move_score(Move m, const Pos *current_board) {
    char target = current_board->b[m.to];
    if (target == '.') return 0; // Not a capture, score is 0
    
    char attacker = current_board->b[m.from];
    int t_val = 0, a_val = 0;
    
    // Convert to uppercase to check piece type regardless of color
    char tu = (char)toupper((unsigned char)target);
    char au = (char)toupper((unsigned char)attacker);
    
    // Simple valuation: P=1, N/B=3, R=5, Q=9, K=100
    if(tu=='P') t_val=1; else if(tu=='N'||tu=='B') t_val=3; else if(tu=='R') t_val=5; else if(tu=='Q') t_val=9; else if(tu=='K') t_val=100;
    if(au=='P') a_val=1; else if(au=='N'||au=='B') a_val=3; else if(au=='R') a_val=5; else if(au=='Q') a_val=9; else if(au=='K') a_val=100;
    
    // MVV-LVA Formula: High target value + low attacker value = best score
    return 1000 + (t_val * 10) - a_val;
}

void sort_moves(Move *moves, int n, const Pos *board) {
    int scores[256];
    for (int i = 0; i < n; i++) scores[i] = move_score(moves[i], board);
    
    // Simple Insertion Sort (Highly efficient for small arrays like chess moves)
    for (int i = 1; i < n; i++) {
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

int quiescence(Pos *board, int alpha, int beta, int maximizing_player) {
    nodes_searched++;
    check_time();
    if (timeout_flag) return 0;

    // "Stand Pat" - The engine assumes it has the option to do nothing (not capture)
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
    
    // Filter out non-captures
    for (int i = 0; i < num_moves; i++) {
        if (board->b[moves[i].to] != '.') caps[num_caps++] = moves[i];
    }
    
    sort_moves(caps, num_caps, board);

    if (maximizing_player) {
        int max_eval = stand_pat;
        for (int i = 0; i < num_caps; i++) {
            Pos next_b = make_move(board, caps[i]);
            int eval_score = quiescence(&next_b, alpha, beta, 0);
            if (eval_score > max_eval) max_eval = eval_score;
            if (alpha < eval_score) alpha = eval_score;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = stand_pat;
        for (int i = 0; i < num_caps; i++) {
            Pos next_b = make_move(board, caps[i]);
            int eval_score = quiescence(&next_b, alpha, beta, 1);
            if (eval_score < min_eval) min_eval = eval_score;
            if (beta > eval_score) beta = eval_score;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

// 2. Baseline Alpha-Beta Pruning (No NMP, No TT, No Sorting)
int alpha_beta(Pos *board, int depth, int alpha, int beta, int maximizing_player) {
    nodes_searched++;
    check_time();
    if (timeout_flag) return 0;

    if (depth == 0) return quiescence(board, alpha, beta, maximizing_player);

    Move moves[256];
    int num_moves = legal_moves(board, moves);
    if (num_moves == 0) return maximizing_player ? -99999 : 99999;

    sort_moves(moves, num_moves, board);
    
    if (maximizing_player) {
        int max_eval = -999999;
        for (int i=0; i<num_moves; i++) {
            Pos next_b = make_move(board, moves[i]);
            int eval_score = alpha_beta(&next_b, depth - 1, alpha, beta, 0);
            if (eval_score > max_eval) max_eval = eval_score;
            if (alpha < eval_score) alpha = eval_score;
            if (beta <= alpha) break;
        }
        return max_eval;
    } else {
        int min_eval = 999999;
        for (int i=0; i<num_moves; i++) {
            Pos next_b = make_move(board, moves[i]);
            int eval_score = alpha_beta(&next_b, depth - 1, alpha, beta, 1);
            if (eval_score < min_eval) min_eval = eval_score;
            if (beta > eval_score) beta = eval_score;
            if (beta <= alpha) break;
        }
        return min_eval;
    }
}

Move find_best_move(Pos *board, double t_limit) {
    search_start_time = get_time_s();
    search_time_limit = t_limit;
    timeout_flag = 0;
    nodes_searched = 0;

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

    uint64_t previous_depth_nodes = 0;

    for (int current_depth = 1; current_depth < 100; current_depth++) {
        uint64_t nodes_before = nodes_searched;
        
        if (timeout_flag) break;
        Move best_this_depth = root_moves[0];
        int best_value = board->white_to_move ? -999999 : 999999;

        for (int i=0; i<num_root; i++) {
            Pos next_b = make_move(board, root_moves[i]);
            int val = alpha_beta(&next_b, current_depth - 1, -999999, 999999, !board->white_to_move);
            if (timeout_flag) break;

            if (board->white_to_move) {
                if (val > best_value) { best_value = val; best_this_depth = root_moves[i]; }
            } else {
                if (val < best_value) { best_value = val; best_this_depth = root_moves[i]; }
            }
        }
        
        if (timeout_flag) break;
        best_overall = best_this_depth;

        // 3. Baseline Metric Tracking
        uint64_t nodes_this_depth = nodes_searched - nodes_before;
        double ebf = 0.0;
        if (current_depth > 1 && previous_depth_nodes > 0) {
            ebf = (double)nodes_this_depth / (double)previous_depth_nodes;
        }
        previous_depth_nodes = nodes_this_depth;

        char uci[6];
        move_to_uci(best_this_depth, uci);
        printf("info depth %d currmove %s\n", current_depth, uci);
        fflush(stdout);

        sprintf(buf, "Reached Depth %d | Best Move: %s | Nodes: %llu | EBF: %.2f\n", 
                current_depth, uci, (unsigned long long)nodes_this_depth, ebf);
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

    double end_time = get_time_s();
    double elapsed_time = end_time - search_start_time;
    if (elapsed_time > 0.0) {
        uint64_t nps = (uint64_t)(nodes_searched / elapsed_time);
        FILE *f = fopen("nps_debug_c.txt", "a");
        if (f) {
            fprintf(f, "NPS: %llu | Total Nodes: %llu\n", (unsigned long long)nps, (unsigned long long)nodes_searched);
            fclose(f);
        }
    }
    return best_overall;
}

// --- STATIC BENCHMARK SUITE ---
void run_static_benchmark(int target_depth) {
    const char *fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", // 1. Start Position
        "r1bq1rk1/1pp2ppp/p1np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 1", // 2. Complex Middlegame
        "8/8/8/4k3/4P3/8/4K3/8 w - - 0 1" // 3. Sparse Endgame
    };

    uint64_t suite_total_nodes = 0;
    double suite_total_time = 0.0;

    printf("\n=== STATIC BENCHMARK SUITE (Target Depth: %d) ===\n", target_depth);

    for (int f = 0; f < 3; f++) {
        Pos pos;
        pos_from_fen(&pos, fens[f]);
        printf("\nBoard %d FEN: %s\n", f + 1, fens[f]);

        uint64_t prev_nodes = 0;
        nodes_searched = 0;
        timeout_flag = 0;
        search_start_time = get_time_s();
        search_time_limit = 9999.0; // Infinite time for the test

        Move root_moves[256];
        int num_root = legal_moves(&pos, root_moves);

        sort_moves(root_moves, num_root, &pos);

        for (int d = 1; d <= target_depth; d++) {
            uint64_t nodes_before = nodes_searched;
            int best_val = pos.white_to_move ? -999999 : 999999;

            for (int i = 0; i < num_root; i++) {
                Pos next_b = make_move(&pos, root_moves[i]);
                int val = alpha_beta(&next_b, d - 1, -999999, 999999, !pos.white_to_move);

                if (pos.white_to_move) {
                    if (val > best_val) best_val = val;
                } else {
                    if (val < best_val) best_val = val;
                }
            }

            uint64_t nodes_this_depth = nodes_searched - nodes_before;
            double ebf = (d > 1 && prev_nodes > 0) ? (double)nodes_this_depth / (double)prev_nodes : 0.0;
            prev_nodes = nodes_this_depth;

            printf(" Depth %d | Nodes: %llu | EBF: %.2f\n", d, (unsigned long long)nodes_this_depth, ebf);
        }

        double elapsed = get_time_s() - search_start_time;
        suite_total_time += elapsed;
        suite_total_nodes += nodes_searched;

        printf(" -> Board %d Time: %.3fs\n", f + 1, elapsed);
    }

    uint64_t suite_nps = (uint64_t)(suite_total_nodes / suite_total_time);
    printf("\n================================================\n");
    printf("FINAL SUITE NPS: %llu nodes/sec\n", (unsigned long long)suite_nps);
    printf("================================================\n\n");
}

// --- Main Engine Loop ---

int main(void) {
    Pos pos;
    pos_start(&pos);

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
            clear_depth_log();
            FILE *f = fopen("nps_debug_c.txt", "w");
            if (f) {
                fprintf(f, "=== NEW GAME STARTED ===\n");
                fclose(f);
            }
            log_msg("Board reset");
        } else if (strcmp(line, "bench") == 0) {
            log_msg("Running static benchmark...");
            run_static_benchmark(4); // Forces exactly Depth 5
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
    return 0;
}