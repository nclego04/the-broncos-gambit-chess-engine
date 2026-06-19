#!/usr/bin/env python3

import sys
import traceback
import time

nodes_searched = 0
search_start_time = 0.0
search_time_limit = 0.0

class SearchTimeout(Exception): pass
# --- Utilities ---

def sq_index(s):
    file = ord(s[0]) - ord('a')
    rank = ord(s[1]) - ord('1')
    return rank * 8 + file

def index_to_sq(idx):
    f = chr((idx % 8) + ord('a'))
    r = chr((idx // 8) + ord('1'))
    return f + r

def print_bestmove(m):
    if m is None:
        print("bestmove 0000")
    else:
        print(f"bestmove {m.to_uci()}")
    sys.stdout.flush()

# --- Data Structures ---

class Move:
    def __init__(self, from_sq, to_sq, promo=''):
        self.from_sq = from_sq
        self.to_sq = to_sq
        self.promo = promo

    def to_uci(self):
        promo_str = self.promo if self.promo else ""
        return f"{index_to_sq(self.from_sq)}{index_to_sq(self.to_sq)}{promo_str}"

    @staticmethod
    def from_uci(uci):
        if not uci or len(uci) < 4: return None
        from_sq = sq_index(uci[0:2])
        to_sq = sq_index(uci[2:4])
        promo = uci[4] if len(uci) >= 5 else ''
        return Move(from_sq, to_sq, promo)

# --- PIECE SQUARE TABLES ---
piece_value = {'P': 100, 'N': 320, 'B': 330, 'R': 500, 'Q': 900, 'K': 20000,
               'p': 100, 'n': 320, 'b': 330, 'r': 500, 'q': 900, 'k': 20000}

pawn_pst = [
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
]

knight_pst = [
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
]

bishop_pst = [
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
]

rook_pst = [
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
]

queen_pst = [
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
]

king_pst = [
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
]

pst_tables = {
    'P': pawn_pst, 'N': knight_pst, 'B': bishop_pst, 
    'R': rook_pst, 'Q': queen_pst, 'K': king_pst
}

class Pos:
    def __init__(self):
        self.b = ['.'] * 64
        self.white_to_move = True

    def copy(self):
        new_pos = Pos()
        new_pos.b = self.b[:]
        new_pos.white_to_move = self.white_to_move
        return new_pos

    def pos_from_fen(self, fen):
        self.b = ['.'] * 64
        self.white_to_move = True
        
        parts = fen.split()
        if not parts: return
        
        placement = parts[0]
        if len(parts) > 1:
            self.white_to_move = (parts[1] == 'w')
            
        rank = 7
        file = 0
        for c in placement:
            if c == '/':
                rank -= 1
                file = 0
            elif c.isdigit():
                file += int(c)
            else:
                idx = rank * 8 + file
                if 0 <= idx < 64:
                    self.b[idx] = c
                file += 1

    def pos_start(self):
        self.pos_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1")

    @staticmethod
    def is_white_piece(c):
        return 'A' <= c <= 'Z'

    # --- Check & Attack Detection ---

    def is_square_attacked(self, sq, by_white):
        r = sq // 8
        f = sq % 8
        if by_white:
            if r > 0 and f > 0 and self.b[(r - 1) * 8 + (f - 1)] == 'P': return True
            if r > 0 and f < 7 and self.b[(r - 1) * 8 + (f + 1)] == 'P': return True
        else:
            if r < 7 and f > 0 and self.b[(r + 1) * 8 + (f - 1)] == 'p': return True
            if r < 7 and f < 7 and self.b[(r + 1) * 8 + (f + 1)] == 'p': return True

        nd = [-17, -15, -10, -6, 6, 10, 15, 17]
        for d in nd:
            to = sq + d
            if 0 <= to < 64:
                tr, tf = to // 8, to % 8
                dr, df = abs(tr - r), abs(tf - f)
                if (dr == 1 and df == 2) or (dr == 2 and df == 1):
                    pc = self.b[to]
                    if by_white and pc == 'N': return True
                    if not by_white and pc == 'n': return True

        dirs = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]
        for di in range(8):
            df, dr = dirs[di][0], dirs[di][1]
            cr, cf = r + dr, f + df
            while 0 <= cr < 8 and 0 <= cf < 8:
                idx = cr * 8 + cf
                pc = self.b[idx]
                if pc != '.':
                    if self.is_white_piece(pc) == by_white:
                        up = pc.upper()
                        rook_dir = (di < 4)
                        bishop_dir = (di >= 4)
                        if up == 'Q': return True
                        if rook_dir and up == 'R': return True
                        if bishop_dir and up == 'B': return True
                        if up == 'K' and abs(cr - r) <= 1 and abs(cf - f) <= 1: return True
                    break
                cr += dr
                cf += df

        for rr in range(r - 1, r + 2):
            for ff in range(f - 1, f + 2):
                if 0 <= rr < 8 and 0 <= ff < 8 and not (rr == r and ff == f):
                    pc = self.b[rr * 8 + ff]
                    if by_white and pc == 'K': return True
                    if not by_white and pc == 'k': return True
        return False

    def in_check(self, white_king):
        k = 'K' if white_king else 'k'
        ksq = -1
        for i in range(64):
            if self.b[i] == k:
                ksq = i
                break
        if ksq < 0: return True
        return self.is_square_attacked(ksq, not white_king)

    def make_move(self, m):
        np = self.copy()
        piece = np.b[m.from_sq]
        np.b[m.from_sq] = '.'
        placed = piece
        if m.promo and piece.upper() == 'P':
            placed = m.promo.upper() if self.is_white_piece(piece) else m.promo.lower()
        np.b[m.to_sq] = placed
        np.white_to_move = not self.white_to_move
        return np

    # --- Move Generation ---

    def gen_pawn(self, from_sq, white):
        moves = []
        r, f = from_sq // 8, from_sq % 8
        direction = 1 if white else -1
        start_r = 1 if white else 6
        prom_r = 7 if white else 0

        nr = r + direction
        if 0 <= nr < 8:
            to = nr * 8 + f
            if self.b[to] == '.':
                promo = 'q' if nr == prom_r else ''
                moves.append(Move(from_sq, to, promo))
                if r == start_r:
                    to2 = (r + 2 * direction) * 8 + f
                    if self.b[to2] == '.':
                        moves.append(Move(from_sq, to2, ''))
            
            for df in [-1, 1]:
                nf = f + df
                if 0 <= nf < 8:
                    to_cap = nr * 8 + nf
                    target = self.b[to_cap]
                    if target != '.' and self.is_white_piece(target) != white:
                        promo = 'q' if nr == prom_r else ''
                        moves.append(Move(from_sq, to_cap, promo))
        return moves

    def gen_knight(self, from_sq, white):
        moves = []
        r, f = from_sq // 8, from_sq % 8
        nd = [(-2,-1), (-2,1), (-1,-2), (-1,2), (1,-2), (1,2), (2,-1), (2,1)]
        for dr, df in nd:
            nr, nf = r + dr, f + df
            if 0 <= nr < 8 and 0 <= nf < 8:
                to = nr * 8 + nf
                target = self.b[to]
                if target == '.' or self.is_white_piece(target) != white:
                    moves.append(Move(from_sq, to, ''))
        return moves

    def gen_sliding(self, from_sq, white, dirs):
        moves = []
        r, f = from_sq // 8, from_sq % 8
        for dr, df in dirs:
            nr, nf = r + dr, f + df
            while 0 <= nr < 8 and 0 <= nf < 8:
                to = nr * 8 + nf
                target = self.b[to]
                if target == '.':
                    moves.append(Move(from_sq, to, ''))
                else:
                    if self.is_white_piece(target) != white:
                        moves.append(Move(from_sq, to, ''))
                    break
                nr += dr
                nf += df
        return moves

    def gen_bishop(self, from_sq, white):
        return self.gen_sliding(from_sq, white, [(1, 1), (1, -1), (-1, 1), (-1, -1)])

    def gen_rook(self, from_sq, white):
        return self.gen_sliding(from_sq, white, [(1, 0), (-1, 0), (0, 1), (0, -1)])

    def gen_queen(self, from_sq, white):
        return self.gen_sliding(from_sq, white, [(1, 1), (1, -1), (-1, 1), (-1, -1), (1, 0), (-1, 0), (0, 1), (0, -1)])

    def gen_king(self, from_sq, white):
        moves = []
        r, f = from_sq // 8, from_sq % 8
        for dr in [-1, 0, 1]:
            for df in [-1, 0, 1]:
                if dr == 0 and df == 0: continue
                nr, nf = r + dr, f + df
                if 0 <= nr < 8 and 0 <= nf < 8:
                    to = nr * 8 + nf
                    target = self.b[to]
                    if target == '.' or self.is_white_piece(target) != white:
                        moves.append(Move(from_sq, to, ''))
        return moves

    def pseudo_legal_moves(self):
        moves = []
        us_white = self.white_to_move
        for i in range(64):
            pc = self.b[i]
            if pc == '.': continue
            white = self.is_white_piece(pc)
            if white != us_white: continue
            
            up = pc.upper()
            if up == 'P': moves.extend(self.gen_pawn(i, white))
            elif up == 'N': moves.extend(self.gen_knight(i, white))
            elif up == 'B': moves.extend(self.gen_bishop(i, white))
            elif up == 'R': moves.extend(self.gen_rook(i, white))
            elif up == 'Q': moves.extend(self.gen_queen(i, white))
            elif up == 'K': moves.extend(self.gen_king(i, white))
        return moves

    def legal_moves(self):
        out = []
        for m in self.pseudo_legal_moves():
            np = self.make_move(m)
            if not np.in_check(not np.white_to_move):
                out.append(m)
        return out

    def apply_uci_move(self, uci):
        m = Move.from_uci(uci)
        if m:
            new_p = self.make_move(m)
            self.b = new_p.b
            self.white_to_move = new_p.white_to_move

    # --- AI & Evaluation (BASELINE) ---

    def evaluate(self):
        white_score = 0
        black_score = 0
        
        for sq in range(64):
            pc = self.b[sq]
            if pc == '.': continue
            
            is_white = self.is_white_piece(pc)
            up_pc = pc.upper()
            
            table = pst_tables.get(up_pc)
            if not table: continue
            
            # Flip the board index if evaluating White (bitwise XOR)
            table_sq = (sq ^ 56) if is_white else sq
            
            # Add Base Material + Square Bonus
            value = piece_value[up_pc] + table[table_sq]
            
            if is_white:
                white_score += value
            else:
                black_score += value
                
        # Absolute score (Positive = White winning, Negative = Black winning)
        return white_score - black_score

    def sort_moves(self, moves):
        vals = {'P': 1, 'N': 3, 'B': 3, 'R': 5, 'Q': 9, 'K': 100, 
                'p': 1, 'n': 3, 'b': 3, 'r': 5, 'q': 9, 'k': 100}
        
        def score(m):
            target = self.b[m.to_sq]
            if target == '.': return 0
            attacker = self.b[m.from_sq]
            return 1000 + (vals.get(target, 0) * 10) - vals.get(attacker, 0)
            
        # Python's built-in Timsort is highly optimized
        moves.sort(key=score, reverse=True)

    def quiescence(self, alpha, beta, maximizing_player):
        global nodes_searched, search_start_time, search_time_limit
        nodes_searched += 1
        if time.time() - search_start_time > search_time_limit: raise SearchTimeout()

        stand_pat = self.evaluate()
        if maximizing_player:
            if stand_pat >= beta: return beta
            if stand_pat > alpha: alpha = stand_pat
        else:
            if stand_pat <= alpha: return alpha
            if stand_pat < beta: beta = stand_pat

        caps = [m for m in self.legal_moves() if self.b[m.to_sq] != '.']

        self.sort_moves(caps) 
        
        if maximizing_player:
            max_eval = stand_pat
            for move in caps:
                eval_score = self.make_move(move).quiescence(alpha, beta, False)
                max_eval = max(max_eval, eval_score)
                alpha = max(alpha, eval_score)
                if beta <= alpha: break
            return max_eval
        else:
            min_eval = stand_pat
            for move in caps:
                eval_score = self.make_move(move).quiescence(alpha, beta, True)
                min_eval = min(min_eval, eval_score)
                beta = min(beta, eval_score)
                if beta <= alpha: break
            return min_eval

    def alpha_beta(self, depth, alpha, beta, maximizing_player):
        global nodes_searched, search_start_time, search_time_limit
        nodes_searched += 1
        
        if time.time() - search_start_time > search_time_limit: raise SearchTimeout()

        if depth == 0:
            return self.quiescence(alpha, beta, maximizing_player)

        moves = self.legal_moves()
        if not moves: return -99999 if maximizing_player else 99999

        self.sort_moves(moves)

        if maximizing_player:
            max_eval = -float('inf')
            for move in moves:
                eval_score = self.make_move(move).alpha_beta(depth - 1, alpha, beta, False)
                max_eval = max(max_eval, eval_score)
                alpha = max(alpha, eval_score)
                if beta <= alpha: break
            return max_eval
        else:
            min_eval = float('inf')
            for move in moves:
                eval_score = self.make_move(move).alpha_beta(depth - 1, alpha, beta, True)
                min_eval = min(min_eval, eval_score)
                beta = min(beta, eval_score)
                if beta <= alpha: break
            return min_eval

    def find_best_move(self, time_limit=9.5):
        global nodes_searched, search_start_time, search_time_limit
        nodes_searched = 0
        search_start_time = time.time()
        search_time_limit = time_limit
        
        root_moves = self.legal_moves()
        if not root_moves: return None

        self.sort_moves(root_moves)
        
        best_move_overall = root_moves[0] 
        
        with open("depth_debug.txt", "a") as log:
            log.write(f"\n--- NEW TURN: Thinking for {time_limit}s ---\n")
            
        previous_depth_nodes = 0
        try:
            for current_depth in range(1, 100):
                nodes_before = nodes_searched
                best_move_this_depth = root_moves[0]
                
                if self.white_to_move:
                    best_value = -float('inf')
                    for move in root_moves:
                        move_value = self.make_move(move).alpha_beta(current_depth - 1, -float('inf'), float('inf'), False)
                        if move_value > best_value:
                            best_value = move_value
                            best_move_this_depth = move
                else:
                    best_value = float('inf')
                    for move in root_moves:
                        move_value = self.make_move(move).alpha_beta(current_depth - 1, -float('inf'), float('inf'), True)
                        if move_value < best_value:
                            best_value = move_value
                            best_move_this_depth = move
                            
                best_move_overall = best_move_this_depth
                
                # ... (Keep all your existing metric tracking code here) ...
                
                # 3. Baseline Metric Tracking
                nodes_this_depth = nodes_searched - nodes_before
                ebf = 0.0
                if current_depth > 1 and previous_depth_nodes > 0:
                    ebf = nodes_this_depth / previous_depth_nodes
                previous_depth_nodes = nodes_this_depth
                
                print(f"info depth {current_depth} currmove {best_move_this_depth.to_uci()}")
                sys.stdout.flush()
                
                with open("depth_debug.txt", "a") as log:
                    log.write(f"Reached Depth {current_depth} | Best Move: {best_move_this_depth.to_uci()} | Nodes: {nodes_this_depth:,} | EBF: {ebf:.2f}\n")
                    
                if best_value >= 90000 or best_value <= -90000:
                    with open("depth_debug.txt", "a") as log:
                        log.write("Forced Checkmate found! Terminating search early.\n")
                    break 

        except SearchTimeout:
            with open("depth_debug.txt", "a") as log:
                if best_move_overall:
                    log.write(f"TIMEOUT! Stopping at depth {current_depth}. Playing: {best_move_overall.to_uci()}\n")
                else:
                    log.write(f"TIMEOUT! Stopping at depth {current_depth}. Playing panic move.\n")

        end_time = time.time()
        elapsed_time = end_time - search_start_time
        if elapsed_time > 0:
            nps = int(nodes_searched / elapsed_time)
            with open("nps_debug_python.txt", "a") as log:
                log.write(f"NPS: {nps:,} | Total Nodes: {nodes_searched:,}\n")

        return best_move_overall

# --- UCI Parser ---

def parse_position(p, line):
    toks = line.split()
    if not toks: return
    
    i = 1
    if i < len(toks) and toks[i] == "startpos":
        p.pos_start()
        i += 1
    elif i < len(toks) and toks[i] == "fen":
        i += 1
        if i + 6 <= len(toks):
            fen = " ".join(toks[i:i+6])
            p.pos_from_fen(fen)
            i += 6
            
    if i < len(toks) and toks[i] == "moves":
        i += 1
        for tok in toks[i:]:
            p.apply_uci_move(tok)

# --- STATIC BENCHMARK SUITE ---
def run_static_benchmark(target_depth):
    global nodes_searched
    fens = [
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1", # 1. Start Position
        "r1bq1rk1/1pp2ppp/p1np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQ1RK1 w - - 0 1", # 2. Complex Middlegame
        "8/8/8/4k3/4P3/8/4K3/8 w - - 0 1" # 3. Sparse Endgame
    ]

    suite_total_nodes = 0
    suite_total_time = 0.0
    
    print(f"\n=== STATIC BENCHMARK SUITE (Target Depth: {target_depth}) ===")
    
    for f_idx, fen in enumerate(fens):
        pos = Pos()
        pos.pos_from_fen(fen)
        print(f"\nBoard {f_idx + 1} FEN: {fen}")
        
        prev_nodes = 0
        nodes_searched = 0
        start_time = time.time()
        
        root_moves = pos.legal_moves()
        
        for d in range(1, target_depth + 1):
            nodes_before = nodes_searched
            best_val = -float('inf') if pos.white_to_move else float('inf')
            
            # 1. Fake an infinite clock so the benchmark never times out
            global search_start_time, search_time_limit
            search_start_time = time.time()
            search_time_limit = 99999.0 
            
            for move in root_moves:
                next_b = pos.make_move(move)
                # 2. Call the unified class method directly!
                val = next_b.alpha_beta(d - 1, -float('inf'), float('inf'), not pos.white_to_move)
                
                if pos.white_to_move:
                    if val > best_val: best_val = val
                else:
                    if val < best_val: best_val = val
                    
            nodes_this_depth = nodes_searched - nodes_before
            ebf = (nodes_this_depth / prev_nodes) if d > 1 and prev_nodes > 0 else 0.0
            prev_nodes = nodes_this_depth
            
            print(f" Depth {d} | Nodes: {nodes_this_depth} | EBF: {ebf:.2f}")
            
        elapsed = time.time() - search_start_time
        suite_total_time += elapsed
        suite_total_nodes += nodes_searched
        
        print(f" -> Board {f_idx + 1} Time: {elapsed:.3f}s")
        
    suite_nps = int(suite_total_nodes / suite_total_time) if suite_total_time > 0 else 0
    print("\n================================================")
    print(f"FINAL SUITE NPS: {suite_nps} nodes/sec")
    print("================================================\n")
    sys.stdout.flush()

def main():
    with open("engine_debug.log", "w") as log:
        def log_msg(msg):
            log.write(msg + "\n")
            log.flush()

        try:
            p = Pos()
            p.pos_start()
            log_msg("Engine successfully launched!")
            
            while True:
                line = sys.stdin.readline()
                if not line:
                    log_msg("EOF received. Connection closed.")
                    break
                    
                line = line.strip()
                if not line: continue
                log_msg(f"Received from Java: {line}")
                
                if line == "uci":
                    print("id name python_engine")
                    print("id author python_dev")
                    print("uciok")
                    sys.stdout.flush()
                    log_msg("Sent uciok")
                    
                elif line == "isready":
                    print("readyok")
                    sys.stdout.flush()
                    log_msg("Sent readyok")
                    
                elif line == "ucinewgame":
                    p.pos_start()
                    log_msg("Board reset")

                    with open("depth_debug.txt", "w") as f:
                        f.write("=== NEW GAME STARTED ===\n")
                    with open("nps_debug_python.txt", "w") as f:
                        f.write("=== NEW GAME STARTED ===\n")
                
                elif line == "bench":
                    log_msg("Running static benchmark...")
                    run_static_benchmark(4) # Set to Depth 4 for Python to avoid long wait times

                elif line.startswith("position"):
                    parse_position(p, line)
                    log_msg("Position parsed")
                    
                elif line.startswith("go"):
                    log_msg("Thinking...")
                    moves = p.legal_moves()
                    if not moves:
                        print_bestmove(None)
                        log_msg("Sent bestmove 0000")
                    else:
                        allocated_time = 9.5
                        toks = line.split()
                        
                        if "movetime" in toks:
                            idx = toks.index("movetime")
                            if idx + 1 < len(toks):
                                requested_ms = int(toks[idx + 1])
                                allocated_time = max(0.1, (requested_ms / 1000.0) - 0.05)
                        else:
                            time_left_ms = None
                            if p.white_to_move and "wtime" in toks:
                                time_left_ms = int(toks[toks.index("wtime") + 1])
                            elif not p.white_to_move and "btime" in toks:
                                time_left_ms = int(toks[toks.index("btime") + 1])
                                
                            if time_left_ms:
                                allocated_time = max(0.1, (time_left_ms / 30000.0) - 0.05)

                        log_msg(f"Allocated {allocated_time:.2f} seconds for this move.")
                        best = p.find_best_move(time_limit=allocated_time)
                        if not best: best = moves[0] 
                            
                        print_bestmove(best)
                        log_msg(f"Sent bestmove {best.to_uci()}")

                elif line == "quit":
                    log_msg("Quit command received.")
                    break

        except Exception as e:
            log_msg(f"\nCRITICAL CRASH: \n{traceback.format_exc()}")

if __name__ == "__main__":
    main()