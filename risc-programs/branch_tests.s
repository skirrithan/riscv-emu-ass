    .text
    .globl _start

_start:
    ADDI x5, x0, 5      # x5 = x0 + 5
    ADDI x6, x0, 5      # x6 = x0 + 5
    ADDI x7, x0, 10     # x7 = x0 + 10
    ADDI x8, x0, 20     # x8 = x0 + 20
    
    BEQ x5, x6, equal_case
    JAL x0, not_equal

equal_case:
    ADD x9, x7, x8      # x9 = x7 + x8 = 30
    JAL x0, end 

not_equal:
    SUB x9, x8, x7       # x9 = x8 - x7 = 10

end:
    JAL x0, end              # infinite loop at end



