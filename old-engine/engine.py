import sys
import traceback
import time


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

        # Pawns
        if by_white:
            if r > 0 and f > 0 and self.b[(r - 1) * 8 + (f - 1)] == 'P': return True
            if r > 0 and f < 7 and self.b[(r - 1) * 8 + (f + 1)] == 'P': return True
        else:
            if r < 7 and f > 0 and self.b[(r + 1) * 8 + (f - 1)] == 'p': return True
            if r < 7 and f < 7 and self.b[(r + 1) * 8 + (f + 1)] == 'p': return True

        # Knights
        nd = [-17, -15, -10, -6, 6, 10, 15, 17]
        for d in nd:
            to = sq + d
            if 0 <= to < 64:
                tr = to // 8
                tf = to % 8
                dr = abs(tr - r)
                df = abs(tf - f)
                if (dr == 1 and df == 2) or (dr == 2 and df == 1):
                    pc = self.b[to]
                    if by_white and pc == 'N': return True
                    if not by_white and pc == 'n': return True

        # Sliders (Bishops, Rooks, Queens)
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

        # King adjacency (extra safety)
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
        r = from_sq // 8
        f = from_sq % 8
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
        r = from_sq // 8
        f = from_sq % 8
        nd = [(-2,-1), (-2,1), (-1,-2), (-1,2), (1,-2), (1,2), (2,-1), (2,1)]
        for dr, df in nd:
            nr, nf = r + dr, f + df
            if 0 <= nr < 8 and 0 <= nf < 8:
                to = nr * 8 + nf
                target = self.b[to]
                if target == '.' or self.is_white_piece(target) != white:
                    moves.append(Move(from_sq, to, ''))
        return moves

    def gen_bishop(self, from_sq, white):
        moves = []
        r = from_sq // 8
        f = from_sq % 8
        dirs = [(1, 1), (1, -1), (-1, 1), (-1, -1)]
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

    def gen_rook(self, from_sq, white):
        moves = []
        r = from_sq // 8
        f = from_sq % 8
        dirs = [(1, 0), (-1, 0), (0, 1), (0, -1)]
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

    def gen_queen(self, from_sq, white):
        moves = []
        r = from_sq // 8
        f = from_sq % 8
        dirs = [(1, 1), (1, -1), (-1, 1), (-1, -1), (1, 0), (-1, 0), (0, 1), (0, -1)]
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

    def gen_king(self, from_sq, white):
        moves = []
        r = from_sq // 8
        f = from_sq % 8
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

    # --- AI & Evaluation ---

    def evaluate(self):
        piece_values = {'P': 100, 'N': 320, 'B': 330, 'R': 500, 'Q': 900, 'K': 20000}
        
        pst_64 = {
            'P': (   0,   0,   0,   0,   0,   0,   0,   0,
                    78,  83,  86,  73, 102,  82,  85,  90,
                     7,  29,  21,  44,  40,  31,  44,   7,
                   -17,  16,  -2,  15,  14,   0,  15, -13,
                   -26,   3,  10,   9,   6,   1,   0, -23,
                   -22,   9,   5, -11, -10,  -2,   3, -19,
                   -31,   8,  -7, -37, -36, -14,   3, -31,
                     0,   0,   0,   0,   0,   0,   0,   0),
            'N': ( -66, -53, -75, -75, -10, -33, -58, -66,
                    -3,  -6, 100, -36,   4,  62,  -4, -14,
                    10,  67,   1,  74,  73,  27,  62,  -2,
                    24,  24,  45,  37,  33,  41,  25,  17,
                    -1,   5,  31,  21,  22,  35,   2,   0,
                   -18,  10,  13,  22,  18,  15,  11, -14,
                   -23, -15,   2,   0,   2,   0, -23, -20,
                   -74, -23, -26, -24, -19, -35, -22, -69),
            'B': ( -59, -78, -82, -76, -23,-107, -37, -50,
                   -11,  20,  35, -42, -39,  31,   2, -22,
                    -9,  39, -32,  41,  52, -10,  28, -14,
                    25,  17,  20,  34,  26,  25,  15,  10,
                    13,  10,  17,  23,  17,  16,   0,   7,
                    14,  25,  24,  15,   8,  25,  20,  15,
                    19,  20,  11,   6,   7,   6,  20,  16,
                    -7,   2, -15, -12, -14, -15, -10, -10),
            'R': (  35,  29,  33,   4,  37,  33,  56,  50,
                    55,  29,  56,  67,  55,  62,  34,  60,
                    19,  35,  28,  33,  45,  27,  25,  15,
                     0,   5,  16,  13,  18,  -4,  -9,  -6,
                   -28, -35, -16, -21, -13, -29, -46, -30,
                   -42, -28, -42, -25, -25, -35, -26, -46,
                   -53, -38, -31, -26, -29, -43, -44, -53,
                   -30, -24, -18,   5,  -2, -18, -31, -32),
            'Q': (   6,   1,  -8,-104,  69,  24,  88,  26,
                    14,  32,  60, -10,  20,  76,  57,  24,
                    -2,  43,  32,  60,  72,  63,  43,   2,
                     1, -16,  22,  17,  25,  20, -13,  -6,
                   -14, -15,  -2,  -5,  -1, -10, -20, -22,
                   -30,  -6, -13, -11, -16, -11, -16, -27,
                   -36, -18,   0, -19, -15, -15, -21, -38,
                   -39, -30, -31, -13, -31, -36, -34, -42),
            'K': (   4,  54,  47, -99, -99,  60,  83, -62,
                   -32,  10,  55,  56,  56,  55,  10,   3,
                   -62,  12, -57,  44, -67,  28,  37, -31,
                   -55,  50,  11,  -4, -19,  13,   0, -49,
                   -55, -43, -52, -28, -51, -47,  -8, -50,
                   -47, -42, -43, -79, -64, -32, -29, -32,
                     4,   3, -14, -50, -57, -18,  13,   4,
                    17,  30,  -3, -14,   6,  -1,  40,  18)
        }
        
        score = 0
        for i in range(64):
            piece = self.b[i]
            if piece == '.': continue

            is_white = self.is_white_piece(piece)
            up = piece.upper()
            
            # Base value
            val = piece_values[up]
            
            # PESTO mapping:
            # White pieces use (7 - rank) * 8 + file
            # Black pieces use rank * 8 + file
            r = i // 8
            f = i % 8
            if is_white:
                pesto_idx = (7 - r) * 8 + f
                val += pst_64[up][pesto_idx]
                score += val
            else:
                pesto_idx = r * 8 + f
                val += pst_64[up][pesto_idx]
                score -= val

        return score

    def find_best_move(self, time_limit=9.5):
        import time
        start_time = time.time()
        
        root_moves = self.legal_moves()
        if not root_moves:
            return None
        best_move_overall = root_moves[0] 
        
        tt = {}
        
        # --- NEW: Grab the real game history built by parse_position ---
        search_history = getattr(self, 'history', [])[:] 
        
        with open("depth_debug.txt", "a") as log:
            log.write(f"\n--- NEW TURN: Thinking for {time_limit}s ---\n")
        
        class SearchTimeout(Exception): pass

        def move_score(m, current_board):
            target = current_board.b[m.to_sq]
            if target == '.': return 0
            attacker = current_board.b[m.from_sq]
            val = {'P': 1, 'N': 3, 'B': 3, 'R': 5, 'Q': 9, 'K': 100,
                   'p': 1, 'n': 3, 'b': 3, 'r': 5, 'q': 9, 'k': 100}
            return 1000 + (val.get(target, 0) * 10) - val.get(attacker, 0)

        def quiescence(board, alpha, beta, maximizing_player):
            if time.time() - start_time > time_limit: raise SearchTimeout()

            stand_pat = board.evaluate()

            if maximizing_player:
                if stand_pat >= beta: return beta
                alpha = max(alpha, stand_pat)
            else:
                if stand_pat <= alpha: return alpha
                beta = min(beta, stand_pat)

            moves = board.legal_moves()
            captures = [m for m in moves if board.b[m.to_sq] != '.']
            captures.sort(key=lambda m: move_score(m, board), reverse=True)

            if maximizing_player:
                max_eval = stand_pat
                for move in captures:
                    eval_score = quiescence(board.make_move(move), alpha, beta, False)
                    max_eval = max(max_eval, eval_score)
                    alpha = max(alpha, eval_score)
                    if beta <= alpha: break
                return max_eval
            else:
                min_eval = stand_pat
                for move in captures:
                    eval_score = quiescence(board.make_move(move), alpha, beta, True)
                    min_eval = min(min_eval, eval_score)
                    beta = min(beta, eval_score)
                    if beta <= alpha: break
                return min_eval

        # --- UPDATED: alpha_beta now tracks the path history externally ---
        def alpha_beta(board, depth, alpha, beta, maximizing_player, hist):
            if time.time() - start_time > time_limit: raise SearchTimeout()

            board_str = "".join(board.b)
            
            # --- REPETITION CHECK ---
            # If we see the same exact board twice in our history line, it's a draw. Reject it!
            if hist.count(board_str) >= 2:
                return 0

            hash_key = board_str + ("W" if board.white_to_move else "B")

            if hash_key in tt:
                entry = tt[hash_key]
                if entry['depth'] >= depth:
                    if entry['type'] == 'EXACT': return entry['value']
                    elif entry['type'] == 'LOWER' and entry['value'] >= beta: return entry['value']
                    elif entry['type'] == 'UPPER' and entry['value'] <= alpha: return entry['value']

            if depth == 0:
                return quiescence(board, alpha, beta, maximizing_player)

            moves = board.legal_moves()
            if not moves: return -99999 if maximizing_player else 99999

            moves.sort(key=lambda m: move_score(m, board), reverse=True)
            original_alpha, original_beta = alpha, beta

            if maximizing_player:
                max_eval = -float('inf')
                for move in moves:
                    next_b = board.make_move(move)
                    
                    # Temporarily add this move to the history list before diving deeper
                    hist.append("".join(next_b.b))
                    eval_score = alpha_beta(next_b, depth - 1, alpha, beta, False, hist)
                    hist.pop() # Remove it as we backtrack
                    
                    max_eval = max(max_eval, eval_score)
                    alpha = max(alpha, eval_score)
                    if beta <= alpha: break
                
                flag = 'EXACT'
                if max_eval <= original_alpha: flag = 'UPPER'
                elif max_eval >= beta: flag = 'LOWER'
                tt[hash_key] = {'depth': depth, 'value': max_eval, 'type': flag}
                return max_eval
            else:
                min_eval = float('inf')
                for move in moves:
                    next_b = board.make_move(move)
                    
                    hist.append("".join(next_b.b))
                    eval_score = alpha_beta(next_b, depth - 1, alpha, beta, True, hist)
                    hist.pop()
                    
                    min_eval = min(min_eval, eval_score)
                    beta = min(beta, eval_score)
                    if beta <= alpha: break
                
                flag = 'EXACT'
                if min_eval >= original_beta: flag = 'LOWER'
                elif min_eval <= alpha: flag = 'UPPER'
                tt[hash_key] = {'depth': depth, 'value': min_eval, 'type': flag}
                return min_eval

        try:
            for current_depth in range(1, 100):
                moves = self.legal_moves()
                if not moves: break
                
                moves.sort(key=lambda m: move_score(m, self), reverse=True)
                best_move_this_depth = moves[0]
                
                if self.white_to_move:
                    best_value = -float('inf')
                    for move in moves:
                        next_b = self.make_move(move)
                        search_history.append("".join(next_b.b))
                        move_value = alpha_beta(next_b, current_depth - 1, -float('inf'), float('inf'), False, search_history)
                        search_history.pop()
                        
                        if move_value > best_value:
                            best_value = move_value
                            best_move_this_depth = move
                else:
                    best_value = float('inf')
                    for move in moves:
                        next_b = self.make_move(move)
                        search_history.append("".join(next_b.b))
                        move_value = alpha_beta(next_b, current_depth - 1, -float('inf'), float('inf'), True, search_history)
                        search_history.pop()
                        
                        if move_value < best_value:
                            best_value = move_value
                            best_move_this_depth = move
                            
                best_move_overall = best_move_this_depth
                
                print(f"info depth {current_depth} currmove {best_move_this_depth.to_uci()}")
                sys.stdout.flush()
                
                with open("depth_debug.txt", "a") as log:
                    log.write(f"Reached Depth {current_depth} | Best Move: {best_move_this_depth.to_uci()}\n")
                    
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

        return best_move_overall

# --- UCI Parser ---

# --- UCI Parser ---

def parse_position(p, line):
    toks = line.split()
    if not toks: return
    
    # --- NEW: Safely inject a history tracker into the Pos object ---
    p.history = []
    
    i = 1
    if i < len(toks) and toks[i] == "startpos":
        p.pos_start()
        p.history.append("".join(p.b)) # Record start state
        i += 1
    elif i < len(toks) and toks[i] == "fen":
        i += 1
        if i + 6 <= len(toks):
            fen = " ".join(toks[i:i+6])
            p.pos_from_fen(fen)
            p.history.append("".join(p.b)) # Record fen state
            i += 6
            
    if i < len(toks) and toks[i] == "moves":
        i += 1
        for tok in toks[i:]:
            p.apply_uci_move(tok) # UNTOUCHED
            p.history.append("".join(p.b)) # Record state after every move

def main():
    # --- CRASH LOGGER SETUP ---
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
                if not line: 
                    continue
                
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
                        # --- DYNAMIC TIME LIMIT PARSER ---
                        allocated_time = 9.5 # Fallback
                        toks = line.split()
                        
                        if "movetime" in toks:
                            # Arena explicitly tells us how long to think
                            idx = toks.index("movetime")
                            if idx + 1 < len(toks):
                                requested_ms = int(toks[idx + 1])
                                allocated_time = max(0.1, (requested_ms / 1000.0) - 0.05)
                        else:
                            # Arena gives us a total game clock (wtime/btime)
                            time_left_ms = None
                            if p.white_to_move and "wtime" in toks:
                                time_left_ms = int(toks[toks.index("wtime") + 1])
                            elif not p.white_to_move and "btime" in toks:
                                time_left_ms = int(toks[toks.index("btime") + 1])
                                
                            if time_left_ms:
                                # We allocate roughly 1/30th of our remaining time to this move
                                # This prevents us from losing on time in long games!
                                allocated_time = max(0.1, (time_left_ms / 30000.0) - 0.05)

                        log_msg(f"Allocated {allocated_time:.2f} seconds for this move.")
                        
                        # ----- THE AI CALL -----
                        best = p.find_best_move(time_limit=allocated_time)
                        
                        # Fallback just in case find_best_move returns None unexpectedly
                        if not best: 
                            best = moves[0] 
                            
                        print_bestmove(best)
                        log_msg(f"Sent bestmove {best.to_uci()}")

                elif line == "quit":
                    log_msg("Quit command received.")
                    break

        except Exception as e:
            # If ANY python crash happens, it writes the exact line and error here!
            log_msg(f"\nCRITICAL CRASH: \n{traceback.format_exc()}")

if __name__ == "__main__":
    main()