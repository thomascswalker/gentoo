# Stages

## Stage 0

This is the pre-bootstrap stage. The compiler is written in C and emits the bare-minimum compiler executable to compile simple Gentoo programs.

## Stage 1

The bootstrap stage. The compiler is written in Gentoo, compiled with the compiler from Stage 0, and emits the same bare-minimum compiler executable from Stage 0.

## Stage 2

The post-bootstrap stage. The compiler is written in Gentoo, compiled with the compiler from Stage 1, and emits the full-featured Gentoo compiler.