#
#  Igel - a UCI chess playing engine derived from GreKo 2018.01
#
#  Copyright (C) 2018 Volodymyr Shcherbyna <volodymyr@shcherbyna.com>
#
#  Igel is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  Igel is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with Igel.  If not, see <http://www.gnu.org/licenses/>.
#

#!/usr/bin/python
 
import chess
import chess.uci
import time

def evaluate(fen):
    # setup engine
    engine = chess.uci.popen_engine("../src/igel")
    
    # setup new game
    engine.uci()
    engine.ucinewgame()
    
    # setup board
    board = chess.Board(fen)
    handler = chess.uci.InfoHandler()
    print(fen)
    print(board)

    # run engine
    engine.info_handlers.append(handler)
    engine.position(board)
    print("starting engine in infinite mode ...")
    search = engine.go(infinite=True, async_callback=True)
    time.sleep(3)
    print("stopping engine ...")
    engine.stop()
    print (handler.info["score"][1])
    return board.san(search.result().bestmove)

def fen_assert(engine_move,  expected_move):
    if engine_move != expected_move:
            raise Exception(engine_move, expected_move)
    
def main():
    fen_assert("Qg8#",  evaluate("rnbqk3/ppppp3/8/6Q1/3P4/4P3/PPP1BPPP/RNB1K1NR w - - 0 1"))

if __name__ == "__main__":
    main()