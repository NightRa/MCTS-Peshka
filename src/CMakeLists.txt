cmake_minimum_required(VERSION 3.3)
project(src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    syzygy/tbcore.cpp
    syzygy/tbcore.h
    syzygy/tbprobe.cpp
    syzygy/tbprobe.h
    benchmark.cpp
    bitbase.cpp
    bitboard.cpp
    bitboard.h
    bitcount.h
    endgame.cpp
    endgame.h
    evaluate.cpp
    evaluate.h
    main.cpp
    material.cpp
    material.h
    misc.cpp
    misc.h
    movegen.cpp
    movegen.h
    movepick.cpp
    movepick.h
    pawns.cpp
    pawns.h
    position.cpp
    position.h
    psqt.cpp
    search.cpp
    search.h
    thread.cpp
    thread.h
    thread_win32.h
    timeman.cpp
    timeman.h
    tt.cpp
    tt.h
    types.h
    uci.cpp
    uci.h
    ucioption.cpp
    mcts.cpp
    mcts.h
    mcts-src.h
    mcts_chess_playing.cpp
    mcts_chess_playing.h
    mcts_tablebase.cpp
    mcts_tablebase.h
    mcts_prior.cpp
    mcts_prior.h mcts_pv.cpp mcts_pv.h)

include_directories(.)
include_directories(syzygy)

add_executable(src ${SOURCE_FILES})