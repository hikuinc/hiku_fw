	.text
	.syntax unified
       .code 16
       .global asm_add_them
       .func  asm_add_them

asm_add_them:
       PUSH   {R5,LR}
       ADD    r0,r1
       POP    {R5,PC}
