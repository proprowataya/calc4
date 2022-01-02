@echo off
cl /EHsc /std:c++17 /Fecalc4.exe /O2 /Oi /GL /GS /MP /W3 /sdl /D NDEBUG /D _CONSOLE /D _UNICODE /D UNICODE Common.cpp Main.cpp StackMachine.cpp Test.cpp Optimizer.cpp SyntaxAnalysis.cpp
