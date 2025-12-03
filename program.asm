bits 64

extern printf

section .data
	message db "This is a test string. :D", 10, 0
	format db "%s", 0

section .text
	global main

main:
	push rbp
	mov rbp, rsp
	sub rsp, 32
	mov rcx, format
	mov rdx, message
	add rsp, 32
	pop rbp
	mov rax, 0
	ret
