# BIF Parser
A lightweight lexer and parser for reading [Bayesian Interchange Format (BIF) files, implemented in modern C++.
This tool parses Bayesian network models defined in `.bif` files, extracting network structure, variables, and probability tables into an internal abstract syntax tree (AST).
It uses two main component:
a very simple lexer, which uses for now std::regex to work
and an hand written recursive descent parser in which I implemented the grammar from the site http://www.cs.washington.edu/dm/vfml/appendixes/bif.htm.
