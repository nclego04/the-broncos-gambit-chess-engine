# UCI Engine Sequence Diagram

Sequence diagram for the chess engine using UCI protocol.  
**Team: The Broncos Gambit - Group 6**

```mermaid
sequenceDiagram
    autonumber
    participant Main as Main loop
    participant UCI as UCI parser
    participant Pos as Position (Board)
    participant Gen as Move generator
    participant Rules as Rule checker
    participant M as Move

    %% --- Handshake: Arena -> Engine ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="uci"
    Main-->>UCI: send("id name MyEngine")
    Main-->>UCI: send("id author TeamX")
    Main-->>UCI: send("uciok")

    %% --- Readiness check ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="isready"
    Main-->>UCI: send("readyok")

    %% --- New game reset ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="ucinewgame"
    Main->>Pos: resetGameState()
    Main->>Gen: clearCaches()
    Main->>Rules: resetState()
    note right of Main: No output required for ucinewgame

    %% --- Position setup: startpos and optional move list ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="position startpos [moves ...]"
    UCI->>Pos: loadStartPos()

    opt moves provided
        loop for each UCI move token (e.g., e2e4)
            UCI->>M: parse(token) -> Move
            UCI->>Rules: validate(Pos, M)
            Rules-->>UCI: legal/illegal
            alt legal
                UCI->>Pos: applyMove(M)
            else illegal
                Main-->>UCI: send("info string illegal move")
            end
        end
    end
    note right of Main: No output required for ucinewgame
    
    %% --- Move computation ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="go movetime T"
    Main->>Gen: generateMoves(Pos)
    Gen-->>Main: candidates(List<Move>)

    loop filter legal moves
        Main->>Rules: validate(Pos, candidate)
        Rules-->>Main: legal/illegal
    end

    alt at least one legal move
        Main->>M: selectBest(candidates)
        Main-->>UCI: send("bestmove " + M.toUci())
    else no legal moves
        Main-->>UCI: send("bestmove 0000")
    end
    
        %% --- Termination ---
    Main->>UCI: readLine()
    UCI-->>Main: cmd="quit"
    Main->>UCI: shutdown()
